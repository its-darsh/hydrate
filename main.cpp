#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/shaders/Textures.hpp>
#include <sstream>
#include <vector>

#define LOG_INFO(message) \
    std::cout << "[HYDRATE][INFO][" << __func__ << "]: " << message << std::endl;
class CWindowTransformer;
inline HANDLE PHANDLE = nullptr;
static std::vector<void*> g_vCallbackIDs;
static std::vector<CWindowTransformer*> g_vTransformers;
static Hyprlang::STRING const* PFRAGMENTSHADER = nullptr;
static Hyprlang::INT* const* PDAMAGEEVERYTHING = nullptr;
static Hyprlang::INT* const* PRENDEREVERYFRAME = nullptr;
static wl_event_source* g_pAnimationTimer = nullptr;
static bool g_bInvalidFragmentShaderFile = false;

std::string loadShaderSourceFromFile(const std::string& path) {
    std::ifstream shaderFile(path);
    if (!shaderFile.good()) {
        HyprlandAPI::addNotification(PHANDLE, "[Hydrate] Error: Shader file not found at " + path, CColor{1.0, 0.2, 0.2, 1.0}, 5000);
        return "";
    }
    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();
    return shaderStream.str();
}

class CTransformerShader : public CShader {
public:
    GLint backgroundTex = -1;
    GLint windowPos = -1;
    GLint windowSize = -1;
    GLint monitorSize = -1;
    GLint borderWidth = -1;
    GLint borderColor = -1;
    GLint time = -1;

    ~CTransformerShader() { destroy(); }
};

class CWindowTransformer : public IWindowTransformer {
public:
    CWindow* pWindow = nullptr;
    SRenderData* p_RenderData = nullptr;
    CTransformerShader m_Shader;
    CFramebuffer m_BackgroundFrameBuffer;
    CFramebuffer m_OutFrameBuffer;
    std::string m_FragmentShaderFile;
    std::string m_CurrentFragmentShaderFile;

    bool compileShaderIfNeeded() {
        if ((this->m_Shader.program != 0) && (this->m_CurrentFragmentShaderFile == this->m_FragmentShaderFile))
            return true;

        if (this->m_Shader.program) {
            this->m_Shader.destroy();
        }

        std::string fragmentSource = loadShaderSourceFromFile(m_FragmentShaderFile);
        if (fragmentSource.empty()) {
            return false;
        }

        g_pHyprRenderer->makeEGLCurrent();
        this->m_Shader.program = g_pHyprOpenGL->createProgram(TEXVERTSRC320, fragmentSource, true);

        if (!this->m_Shader.program) {
            HyprlandAPI::addNotification(PHANDLE, "[Hydrate] Shader compilation failed! Check logs.", CColor{1.0, 0.2, 0.2, 1.0}, 5000);
            g_bInvalidFragmentShaderFile = true;  // remove this to demolish your computer forever/s
            return false;
        }

        this->m_CurrentFragmentShaderFile = this->m_FragmentShaderFile;

        this->m_Shader.posAttrib = glGetAttribLocation(this->m_Shader.program, "pos");
        this->m_Shader.texAttrib = glGetAttribLocation(this->m_Shader.program, "texcoord");
        this->m_Shader.proj = this->m_Shader.getUniformLocation("proj");
        this->m_Shader.tex = this->m_Shader.getUniformLocation("tex");
        this->m_Shader.backgroundTex = this->m_Shader.getUniformLocation("backgroundTex");
        this->m_Shader.windowPos = this->m_Shader.getUniformLocation("windowPos");
        this->m_Shader.windowSize = this->m_Shader.getUniformLocation("windowSize");
        this->m_Shader.monitorSize = this->m_Shader.getUniformLocation("monitorSize");
        this->m_Shader.borderColor = this->m_Shader.getUniformLocation("borderColor");
        this->m_Shader.borderWidth = this->m_Shader.getUniformLocation("borderWidth");
        this->m_Shader.time = this->m_Shader.getUniformLocation("time");
        return true;
    }

    void preWindowRender(SRenderData* renderData) {
        this->p_RenderData = renderData;
        if (g_bInvalidFragmentShaderFile)
            return;

        auto* const PWINDOW = this->p_RenderData->pWindow;
        auto* const PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

        // ensure our framebuffer is ready
        if (!this->m_BackgroundFrameBuffer.isAllocated() || this->m_BackgroundFrameBuffer.m_vSize != PMONITOR->vecPixelSize) {
            if (this->m_BackgroundFrameBuffer.isAllocated())
                this->m_BackgroundFrameBuffer.release();
            this->m_BackgroundFrameBuffer.alloc(PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y, PMONITOR->drmFormat);
        }

        CFramebuffer* pLastFB = g_pHyprOpenGL->m_RenderData.currentFB;
        this->m_BackgroundFrameBuffer.bind();

        CBox monBox = {0, 0, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y};

        if (PWINDOW->m_bIsFloating && g_pHyprOpenGL->m_RenderData.mainFB && g_pHyprOpenGL->m_RenderData.mainFB->m_cTex.m_iTexID != 0) {
            // pass everything "behind" the window
            g_pHyprOpenGL->renderTexture(g_pHyprOpenGL->m_RenderData.mainFB->m_cTex, &monBox, 1.f, 0, false, false);
            pLastFB->bind();
            return;
        }

        // (EVIL) for tiled windows we need to reconstruct the background texture ourself
        SRenderModifData lastModif = g_pHyprOpenGL->m_RenderData.renderModif;
        g_pHyprOpenGL->m_RenderData.renderModif.enabled = true;
        g_pHyprOpenGL->m_RenderData.renderModif.modifs.clear();
        g_pHyprOpenGL->m_RenderData.renderModif.modifs.push_back({SRenderModifData::RMOD_TYPE_TRANSLATE, -PMONITOR->vecPosition});

        auto TEXIT = g_pHyprOpenGL->m_mMonitorBGFBs.find(PMONITOR);
        if (TEXIT != g_pHyprOpenGL->m_mMonitorBGFBs.end()) {
            g_pHyprOpenGL->renderTexture(TEXIT->second.m_cTex, &monBox, 1.f, 0, false, false);
        }

        timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        // userland layers
        for (auto& ls : PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) {
            g_pHyprRenderer->renderLayer(ls.get(), PMONITOR, &now);
        }
        for (auto& ls : PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) {
            g_pHyprRenderer->renderLayer(ls.get(), PMONITOR, &now);
        }

        // restore
        g_pHyprOpenGL->m_RenderData.renderModif = lastModif;
        // end evil hack

        pLastFB->bind();
    }

    CFramebuffer* transform(CFramebuffer* in) {
        if (g_bInvalidFragmentShaderFile || !this->compileShaderIfNeeded() || (this->p_RenderData && this->p_RenderData->popup) || (this->p_RenderData->pWindow->m_bIsFullscreen)) {
            return in;
        }

        auto* const PWINDOW = this->p_RenderData->pWindow;
        auto* const PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

        if (!this->m_OutFrameBuffer.isAllocated() || this->m_OutFrameBuffer.m_vSize != PMONITOR->vecPixelSize) {
            this->m_OutFrameBuffer.m_pStencilTex = &g_pHyprOpenGL->m_mMonitorRenderResources[PMONITOR].stencilTex;
            this->m_OutFrameBuffer.alloc(PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y, PMONITOR->drmFormat);
        }

        this->m_OutFrameBuffer.bind();
        g_pHyprOpenGL->clear(CColor(0, 0, 0, 0));

        float matrix[9];
        CBox monBox = {0, 0, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y};
        wlr_matrix_project_box(matrix, monBox.pWlr(), WL_OUTPUT_TRANSFORM_NORMAL, 0, g_pHyprOpenGL->m_RenderData.projection);

        glUseProgram(this->m_Shader.program);

#ifndef GLES2
        glUniformMatrix3fv(this->m_Shader.proj, 1, GL_TRUE, matrix);
#else
        wlr_matrix_transpose(matrix, matrix);
        glUniformMatrix3fv(this->m_Shader.proj, 1, GL_FALSE, matrix);
#endif

        CBox windowBox = PWINDOW->getFullWindowBoundingBox();
        windowBox.translate(-PMONITOR->vecPosition).scale(PMONITOR->scale);

        glUniform2f(this->m_Shader.monitorSize, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y);
        glUniform2f(this->m_Shader.windowPos, windowBox.x, windowBox.y);
        glUniform2f(this->m_Shader.windowSize, windowBox.width, windowBox.height);
        glUniform1f(this->m_Shader.borderWidth, PWINDOW->getRealBorderSize() * PMONITOR->scale);
        glUniform1f(this->m_Shader.time, g_pHyprRenderer->m_tRenderTimer.getSeconds());

        const CGradientValueData& borderColorData = PWINDOW->m_cRealBorderColor;
        if (!borderColorData.m_vColors.empty()) {
            const CColor& currentColor = borderColorData.m_vColors[0];
            glUniform4f(this->m_Shader.borderColor, currentColor.r, currentColor.g, currentColor.b, currentColor.a);
        } else {
            glUniform4f(this->m_Shader.borderColor, 0.0f, 0.0f, 0.0f, 0.0f);
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(in->m_cTex.m_iTarget, in->m_cTex.m_iTexID);
        glTexParameteri(in->m_cTex.m_iTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(in->m_cTex.m_iTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glUniform1i(this->m_Shader.tex, 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(this->m_BackgroundFrameBuffer.m_cTex.m_iTarget, this->m_BackgroundFrameBuffer.m_cTex.m_iTexID);
        glTexParameteri(this->m_BackgroundFrameBuffer.m_cTex.m_iTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(this->m_BackgroundFrameBuffer.m_cTex.m_iTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glUniform1i(this->m_Shader.backgroundTex, 1);

        glVertexAttribPointer(this->m_Shader.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
        glVertexAttribPointer(this->m_Shader.texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

        glEnableVertexAttribArray(this->m_Shader.posAttrib);
        glEnableVertexAttribArray(this->m_Shader.texAttrib);

        g_pHyprOpenGL->blend(false);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisableVertexAttribArray(this->m_Shader.posAttrib);
        glDisableVertexAttribArray(this->m_Shader.texAttrib);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(this->m_BackgroundFrameBuffer.m_cTex.m_iTarget, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(in->m_cTex.m_iTarget, 0);

        g_pHyprOpenGL->blend(true);
        return &this->m_OutFrameBuffer;
    }

    ~CWindowTransformer() {
        if (!this->m_Shader.program)
            return;
        g_pHyprRenderer->makeEGLCurrent();
        this->m_Shader.destroy();
    }
};

int animationTimerTicked(void* data) {
    if (!(g_pAnimationTimer && **PRENDEREVERYFRAME)) {
        wl_event_source_timer_update(g_pAnimationTimer, 0);
        return 0;
    }

    for (const auto& pTransformer : g_vTransformers) {
        if (!(pTransformer && pTransformer->pWindow && pTransformer->pWindow->m_bIsMapped && !pTransformer->pWindow->isHidden()))
            continue;

        if (**PDAMAGEEVERYTHING) {
            auto* const PMONITOR = g_pCompositor->getMonitorFromID(pTransformer->pWindow->m_iMonitorID);
            g_pHyprRenderer->damageMonitor(PMONITOR);
            continue;
        }

        g_pHyprRenderer->damageWindow(pTransformer->pWindow, true);
    }

    wl_event_source_timer_update(g_pAnimationTimer, (g_pCompositor && g_pHyprRenderer->m_pMostHzMonitor) ? (1000.0 / g_pHyprRenderer->m_pMostHzMonitor->refreshRate) : 16);

    return 0;
}

void onNewWindow(void* self, std::any data) {
    auto* const PWINDOW = std::any_cast<CWindow*>(data);

    auto pTransformer = static_cast<CWindowTransformer*>(PWINDOW->m_vTransformers.emplace_back(std::make_unique<CWindowTransformer>()).get());

    pTransformer->pWindow = PWINDOW;
    pTransformer->m_FragmentShaderFile = std::string{*PFRAGMENTSHADER};

    g_vTransformers.push_back(pTransformer);
}

void onWindowClose(void* self, std::any data) {
    auto* const PWINDOW = std::any_cast<CWindow*>(data);
    std::erase_if(g_vTransformers, [PWINDOW](CWindowTransformer* pTransformer) {
        return pTransformer->pWindow == PWINDOW;
    });
}

void onConfigReload(void* self, std::any data) {
    LOG_INFO(std::format("RENDEREVERYFRAME={} DAMAGEEVERYTHING={} FRAGMENTSHADER={}", **PRENDEREVERYFRAME, **PDAMAGEEVERYTHING, *PFRAGMENTSHADER));

    g_bInvalidFragmentShaderFile = (loadShaderSourceFromFile(*PFRAGMENTSHADER).empty());  // enough to display a message about a missing file

    for (const auto& pTransformer : g_vTransformers) {
        if (!pTransformer || !pTransformer->pWindow)
            continue;

        pTransformer->m_FragmentShaderFile = std::string{*PFRAGMENTSHADER};
        pTransformer->compileShaderIfNeeded();
    }

    if (!g_pAnimationTimer)
        return;

    if (!**PRENDEREVERYFRAME) {
        wl_event_source_timer_update(g_pAnimationTimer, 0);  // 0 to dismiss the timer
        return;
    }

    wl_event_source_timer_update(g_pAnimationTimer, (g_pCompositor && g_pHyprRenderer->m_pMostHzMonitor) ? (1000.0 / g_pHyprRenderer->m_pMostHzMonitor->refreshRate) : 16);
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;  // most compatible hyprland plugin, check
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    LOG_INFO("begin initialization...");

    g_vCallbackIDs.push_back(HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, SCallbackInfo& info, std::any data) { onNewWindow(self, data); }));
    g_vCallbackIDs.push_back(HyprlandAPI::registerCallbackDynamic(PHANDLE, "closeWindow", [&](void* self, SCallbackInfo& info, std::any data) { onWindowClose(self, data); }));
    g_vCallbackIDs.push_back(HyprlandAPI::registerCallbackDynamic(PHANDLE, "configReloaded", [&](void* self, SCallbackInfo& info, std::any data) { onConfigReload(self, data); }));

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hydrate:fragment-shader", Hyprlang::STRING{""});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hydrate:damage-everything", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hydrate:render-every-frame", Hyprlang::INT{0});

    PFRAGMENTSHADER = (Hyprlang::STRING const*)(HyprlandAPI::getConfigValue(PHANDLE, "plugin:hydrate:fragment-shader")->getDataStaticPtr());  // casting this back results in a char *
    PDAMAGEEVERYTHING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hydrate:damage-everything")->getDataStaticPtr();
    PRENDEREVERYFRAME = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hydrate:render-every-frame")->getDataStaticPtr();

    // not super sure if hyprland does have its own idle source loop so i'm using this for now
    g_pAnimationTimer = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, animationTimerTicked, nullptr);

    // hyprland doesn't do an initial call, one of many hacks
    onConfigReload(nullptr, nullptr);

    HyprlandAPI::addNotification(PHANDLE, "[Hydrate] Initialized successfully!", CColor{0.2, 1.0, 0.2, 1.0}, 5000);

    LOG_INFO("initialized and working, somehow...");

    return {"hydrate", "window shaders and stuff", "Yousef EL-Darsh", "0.0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    LOG_INFO("begin unloading...");

    if (g_pAnimationTimer) {
        wl_event_source_remove(g_pAnimationTimer);
        g_pAnimationTimer = nullptr;
    }

    for (const auto& id : g_vCallbackIDs) {
        HyprlandAPI::unregisterCallback(PHANDLE, (HOOK_CALLBACK_FN*)id);
    }

    for (auto& pWindow : g_pCompositor->m_vWindows) {
        if (!pWindow || !pWindow->m_bIsMapped)
            continue;

        std::erase_if(pWindow->m_vTransformers, [](const std::unique_ptr<IWindowTransformer>& pTransformer) {
            return std::find(g_vTransformers.begin(), g_vTransformers.end(), pTransformer.get()) != g_vTransformers.end();
        });
    }

    g_vTransformers.clear();

    HyprlandAPI::addNotification(PHANDLE, "[Hydrate] Unloaded successfully!", CColor{0.2, 1.0, 0.2, 1.0}, 5000);
    LOG_INFO("unloading done. goodbye!");
}
