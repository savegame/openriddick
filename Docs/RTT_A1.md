# RTT_A1 — Direct Render: инвентарь пост-процесс квадов и план отключения

Исследование для флага «direct render» (отключение всех post-process квадов с текстурами).
Движок: XR (CXR_EngineImpl) → CXR_VBManager → CRC_GLES3. Дата: 2026-07-22.

## (a) Инвентарь квадов/блитов в кадровом пути

| # | Квад | Файл:строка | Тип | Что делает |
|---|---|---|---|---|
| B1 | PresentToWindow composite | RenderContexts/GLES3/MDisplaySDL2.cpp:678-730 (шейдер 193-213, VBO 998) | backend FS-quad | TRIANGLE_STRIP(4) из `m_CompVBO`, сэмплит `m_ScreenColorTex` в FBO 0 с поворотом `uRot` |
| B2 | RTT overlay grid | GLES3/GLES3_RTTOverlay.cpp:64,115 (вызов MDisplaySDL2.cpp:710-725) | backend debug quads | сетка живых RTT-FBO поверх окна (RIDDICK_DBG_RTT=1) |
| B3 | CopyToTexture blit | MDisplaySDL2.cpp:1263-1335 | блит (не квад) | glBlitFramebuffer/glCopyTexSubImage2D из текущего FB в текстуру слота |
| E1 | Screen-capture для PP | XR/XREngine.cpp:4410, 4502 | CopyToTexture | копия бэкбуфера → TextureID_Screen (TCScreen id 0) |
| E2 | Motion-blur quad | XREngine.cpp:4489 | VBM_RenderRect | радиальный blur по TextureID_Screen + MotionMap |
| E3 | Histogram/exposure ×8 | XREngine.cpp:4581 (+feedback 4596) | VBM_RenderRect + occ-query | FP20 "XREngine_Histogram", динамическая экспозиция |
| E4 | Gaussian glow passes | XR/XRUtil.cpp:3073-3274 (shrink 3115/3124, X 3247/3256, Y 3266/3274); вызовы XREngine.cpp:4441, 4685 | VBM_RenderRect + CopyToTexture | shrink + двухпроходный gauss в BlurScreen (TCScreen id 6) |
| E5 | Color-correction build | XREngine.cpp:4191-4316 (quads 4264/4288, copies 4184-4188, 4314) | VBM_RenderRect + copies | hexagon-квады в куб-мапы цветокоррекции |
| E6 | FINAL PP quad | XREngine.cpp:4721 (textures 4754-4760) | VBM_RenderRect | FS-квад: Screen+Blur+ColorCorr, FP20 XREngine_Final*/Linear — экспозиция/глоу/тонмап |
| E7 | Widescreen borders | XREngine.cpp:4784, 4789 | VBM_RenderRect | чёрные полосы (без текстуры) |
| E8 | Camera effects (DV) | GameWorld/WClientMod_DV.cpp: копии 129/215/275/505/571 + квады эффектов | RenderRect + copies | eyeshine/radial blur/visions (CreepingDark, AncientWeapon) |
| G1 | Cube menu FS images/blur | GameWorld/WFrontEndMod_Cube.cpp: RenderFullScreenImage 3778; blur 3526/3634/3715; journal 6179/6289/6367; projection 3452+ | RenderRect + copies | фон куб-меню, glow, журнал — источник ~30 копий/кадр в меню |
| G2 | Menu screen quads | GameWorld/WFrontEndMod_Menus.cpp:2841, 2904, 5518, 5628, 5803, 6104, 6154, 6532 | VBM_RenderRect prio 0.1f | текстурированные UI-картинки меню (НЕ пост-процесс) |
| G3 | Menu screen grabs | WFrontEndMod_Menus.cpp:5857; WClientMod.cpp:800; GameClasses/WObj_CharRender.cpp:2422/2453 | CopyToTexture | захват экрана для фонов/затемнений GUI |
| M1 | Portal/mirror capture | XREngine.cpp:2894 | CopyToTexture | сцена портала → pPortal->m_TextureID |
| M2 | ResolveScreen | XREngine.cpp:3019, 3920; GameClasses/Models/WModel_EffectSystem.cpp:1654 | CopyToTexture | снапшот экрана для distortion/heat-эффектов |
| M3 | Deferred/shadow/depth | XREngine.cpp:3786, 3896-3906, 3949, 3955 | CopyToTexture | G-buffer/shadowmask/depth — только при соотв. shader traits |
| M4 | Envmap cube | XREngine.cpp:4184-4188 | RenderRect + copies | 128×128 env-capture |
| M5 | Misc coverage | XRModels/Model_TriMesh/WTriMesh.cpp:6024; GameClasses/WObj_Misc/WObj_TentacleSystem_Render.cpp:288/304/385 | CopyToTexture | coverage/tentacle temporaries |

## (b) Путь сабмита (единый для E/G/M)

Все движковые квады строятся через `CXR_Util::VBM_RenderRect` (XR/XRUtil.cpp:2589, 2638): 4 вершины, умноженные на 2D-матрицу `Alloc_M4_Proj2DRelBackPlane`, `Render_IndexedTriangles(m_lQuadParticleTriangles, 2)` (XRUtil.cpp:2609), приоритет в `pVB->m_Priority`, сабмит `m_pVBM->AddVB`. Копии идут через `CXR_VBManager::AddCopyToTexture` (XR/XRVBManager.cpp:1800-1813) — это PreRender-callback, не геометрия.

Кадровая цепочка: `CXR_Application::Render` XRApp.cpp:5729 `BeginScene` → `pVBM->Render(pRC,0)` (5744/5753) — сортировка по (scope, priority); PreRender-колбэки (`CXR_PreRenderData_RenderTarget_CopyToTexture1::RenderTarget_CopyToTexture`, XR/XRVBUtil.h:69-73) → `RC->RenderTarget_CopyToTexture`; VB → `CRC_GLES3::Render_VertexBuffer` (MDisplaySDL2.cpp:2631). `EndScene` (5778) → `pDC->PageFlip()` (XRApp.cpp:5837).

Вызов PP: `CWClient_Mod::Render_GUI` → `Engine_PostProcess` (WClientMod.cpp:781); camera effects — `CXR_Model_CameraEffects` (WClientMod_DV.cpp); GUI — `CGameContextMod::RenderGUI` (Exe/WGameContextMain.cpp:913).

## (c) Финальный путь «на экран»

`CDisplayContextSDL2::PageFlip` (MDisplaySDL2.cpp:325-345) → `CRC_GLES3::PresentToWindow` (678): bind FBO 0, viewport окна, стейт off, композит-шейдер (193-213), `glDrawArrays(TRIANGLE_STRIP,0,4)` (706), затем оверлей B2, затем `SDL_GL_SwapWindow` (336). Весь движок рисует «бэкбуфер» в screen-FBO (`BindScreenTarget` 659-671, FBO в `EnsureScreenFBO` 598). Т.е. **картинка доходит до окна ровно один раз — через B1**; E6 лишь перерисовывает сцену внутри screen-FBO с экспозицией.

## (d) План гейтинга флагом (RIDDICK_DIRECT_RENDER=1)

ВАЖНО: имя уже занято бэкендом (MDisplaySDL2.cpp:1226-1241 — force SetRenderTarget→screen FBO; 1270 — skip CopyToTexture). Либо расширять его, либо новое имя.

Вердикты:
- **SKIP safe (картинка не пострадает)**: E2,E3,E4,E5,E6,E7,E8 (PP и camera effects — сцена уже лежит в screen-FBO, E6 только тонирует), B2 (debug), G1-blur/journal, M2-M5 (distortion/deferred/env — деградируют эффекты, не основное изображение).
- **SKIP unsafe**: B1 — единственный путь в окно; M1 — зеркала/порталы станут placeholder (допустимо для диагностики); G2/G3 — убьёт UI.
- **Правильный direct-render target**: default framebuffer 0 в оконном разрешении, не screen-FBO. Изменения: `BindScreenTarget` (659) → `glBindFramebuffer(0)` + viewport окна; `PresentToWindow` (678) → ранний return (без композита и ребинда); `ScreenH()` (593) → высота окна для Y-flip; поворот обязан быть 0 (иначе FBO+композит остаются обязательными).
- **Гейты**: точка №1 — ранний return в `CXR_EngineImpl::Engine_PostProcess` (XREngine.cpp:4339) — выключает E1-E7 целиком; точка №2 — skip вызова camera effects (WClientMod_DV); точка №3 — backend (уже есть). Копии E1/M2 авто-исчезают вместе с гейтом их вызывателей; portal-captures (M1) гейтить отдельно подфлагом, т.к. ломают зеркала.
