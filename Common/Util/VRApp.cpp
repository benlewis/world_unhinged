//
//  VRApp.cpp
//  osx_world_vr_app
//
//  Created by Ben Lewis on 7/11/14.
//  Copyright (c) 2014 Ben Lewis Personal. All rights reserved.
//

#include "VRApp.h"

#include "LevelGame.h"

//-------------------------------------------------------------------------------------
// ***** VRApp

VRApp::VRApp()
: pRender(0),
WindowSize(1280,800),
ScreenNumber(0),
FirstScreenInCycle(0),
Hmd(0),
StartSensorCaps(0),
UsingDebugHmd(false),
LoadingState(LoadingState_Frame0),
HaveVisionTracking(false),
HmdStatus(0),

// Initial location
DistortionClearBlue(0),
ShiftDown(false),
CtrlDown(false),
HmdSettingsChanged(false),

// Modifiable options.
RendertargetIsSharedByBothEyes(false),
DynamicRezScalingEnabled(false),
ForceZeroIpd(false),
DesiredPixelDensity(1.0f),
FovSideTanMax(1.0f), // Updated based on Hmd.
TimewarpEnabled(true),
TimewarpRenderIntervalInSeconds(0.0f),
FreezeEyeUpdate(false),
FreezeEyeOneFrameRendered(false),
CenterPupilDepthMeters(0.05f),
ForceZeroHeadMovement(false),
VsyncEnabled(true),
MultisampleEnabled(false),
IsLowPersistence(true),
DynamicPrediction(true),
PositionTrackingEnabled(true),

// Scene state
SceneMode(Scene_World),
GridDisplayMode(GridDisplay_None),
GridMode(Grid_Lens),
TextScreen(Text_None),
BlocksShowType(0),
BlocksCenter(0.0f, 0.0f, 0.0f)
{
  
  FPS             = 0.0f;
  SecondsPerFrame = 0.0f;
  FrameCounter    = 0;
  LastFpsUpdate   = 0;
  
  EyeRenderSize[0] = EyeRenderSize[1] = Sizei(0);
  
  DistortionClearBlue = false;
}

VRApp::~VRApp()
{
  CleanupDrawTextFont();
  
  if (Hmd)
  {
    ovrHmd_Destroy(Hmd);
    Hmd = 0;
  }
  
  CollisionModels.ClearAndRelease();
  GroundCollisionModels.ClearAndRelease();
  
  ovr_Shutdown();
}


int VRApp::OnStartup(int argc, const char** argv)
{
  
  // *** Oculus HMD & Sensor Initialization
  
  // Create DeviceManager and first available HMDDevice from it.
  // Sensor object is created from the HMD, to ensure that it is on the
  // correct device.
  
  ovr_Initialize();
  
  Hmd = ovrHmd_Create(0);
  
  if (!Hmd)
  {
    // If we didn't detect an Hmd, create a simulated one for debugging.
    Hmd           = ovrHmd_CreateDebug(ovrHmd_DK1);
    UsingDebugHmd = true;
    if (!Hmd)
    {   // Failed Hmd creation.
      return 1;
    }
  }
  
  // Get more details about the HMD.
  ovrHmd_GetDesc(Hmd, &HmdDesc);
  
  WindowSize = HmdDesc.Resolution;
  
  
  // ***** Setup System Window & rendering.
  
  if (!SetupWindowAndRendering(argc, argv))
    return 1;
  
  // Initialize FovSideTanMax, which allows us to change all Fov sides at once - Fov
  // starts at default and is clamped to this value.
  FovSideTanLimit = FovPort::Max(HmdDesc.MaxEyeFov[0], HmdDesc.MaxEyeFov[1]).GetMaxSideTan();
  FovSideTanMax   = FovPort::Max(HmdDesc.DefaultEyeFov[0], HmdDesc.DefaultEyeFov[1]).GetMaxSideTan();
  
  PositionTrackingEnabled = (HmdDesc.SensorCaps & ovrSensorCap_Position) ? true : false;
  
  
  // *** Configure HMD Stereo settings.
  
  CalculateHmdValues();
  
  // Query eye height.
  ThePlayer.UserEyeHeight = ovrHmd_GetFloat(Hmd, OVR_KEY_EYE_HEIGHT, ThePlayer.UserEyeHeight);
  ThePlayer.BodyPos.y     = ThePlayer.UserEyeHeight;
  // Center pupil for customization; real game shouldn't need to adjust this.
  CenterPupilDepthMeters  = ovrHmd_GetFloat(Hmd, "CenterPupilDepth", 0.0f);
  
  
  ThePlayer.bMotionRelativeToBody = false;  // Default to head-steering for DK1
  
  if (UsingDebugHmd)
    Menu.SetPopupMessage("NO HMD DETECTED");
  else if (!(ovrHmd_GetSensorState(Hmd, 0.0f).StatusFlags & ovrStatus_OrientationTracked))
    Menu.SetPopupMessage("NO SENSOR DETECTED");
  else
    Menu.SetPopupMessage("Press F9 for Full-Screen on Rift");
  // Give first message 10 sec timeout, add border lines.
  Menu.SetPopupTimeout(10.0f, true);
  
  PopulateOptionMenu();
  
  // *** Identify Scene File & Prepare for Loading
  
  InitMainFilePath();
  PopulatePreloadScene();
  
  LastUpdate = ovr_GetTimeInSeconds();
  
  return 0;
}


bool VRApp::SetupWindowAndRendering(int argc, const char** argv)
{
  // *** Window creation
  
  if (!pPlatform->SetupWindow(WindowSize.w, WindowSize.h))
    return false;
  
  // Report relative mouse motion in OnMouseMove
  pPlatform->SetMouseMode(Mouse_Relative);
  
  
  // *** Initialize Rendering
  
#if defined(OVR_OS_WIN32)
  const char* graphics = "d3d11";
#else
  const char* graphics = "GL";
#endif
  
  // Select renderer based on command line arguments.
  for(int i = 1; i < argc; i++)
  {
    if(!strcmp(argv[i], "-r") && i < argc - 1)
    {
      graphics = argv[i + 1];
    }
    else if(!strcmp(argv[i], "-fs"))
    {
      RenderParams.Fullscreen = true;
    }
  }
  
  String title = "Oculus World Demo ";
  title += graphics;
  
  if (HmdDesc.ProductName[0])
  {
    title += " : ";
    title += HmdDesc.ProductName;
  }
  pPlatform->SetWindowTitle(title);
  
  // Enable multi-sampling by default.
  RenderParams.Display     = DisplayId(HmdDesc.DisplayDeviceName, HmdDesc.DisplayId);
  RenderParams.Multisample = 1;
  //RenderParams.Fullscreen = true;
  pRender = pPlatform->SetupGraphics(OVR_DEFAULT_RENDER_DEVICE_SET,
                                     graphics, RenderParams);
  if (!pRender)
    return false;
  
  return true;
}

// Custom formatter for Timewarp interval message.
static String FormatTimewarp(OptionVar* var)
{
  char    buff[64];
  float   timewarpInterval = *var->AsFloat();
  OVR_sprintf(buff, sizeof(buff), "%.1fms, %.1ffps",
              timewarpInterval * 1000.0f,
              ( timewarpInterval > 0.000001f ) ? 1.0f / timewarpInterval : 10000.0f);
  return String(buff);
}

static String FormatMaxFromSideTan(OptionVar* var)
{
  char   buff[64];
  float  degrees = 2.0f * atan(*var->AsFloat()) * (180.0f / Math<float>::Pi);
  OVR_sprintf(buff, sizeof(buff), "%.1f Degrees", degrees);
  return String(buff);
}


void VRApp::PopulateOptionMenu()
{
  // For shortened function member access.
  typedef VRApp OWD;
  
  // *** Scene Content Sub-Menu
  
  // Test
  /*
   Menu.AddEnum("Scene Content.EyePoseMode", &FT_EyePoseState).AddShortcutKey(Key_Y).
   AddEnumValue("Separate Pose",  0).
   AddEnumValue("Same Pose",      1).
   AddEnumValue("Same Pose+TW",   2);
   */
  
  // Navigate between scenes.
  Menu.AddEnum("Scene Content.Rendered Scene ';'", &SceneMode).AddShortcutKey(Key_Semicolon).
  AddEnumValue("World",        Scene_World).
  AddEnumValue("Cubes",        Scene_Cubes).
  AddEnumValue("Oculus Cubes", Scene_OculusCubes);
  // Animating blocks
  Menu.AddEnum("Scene Content.Animated Blocks", &BlocksShowType).
  AddShortcutKey(Key_B).SetNotify(this, &OWD::BlockShowChange).
  AddEnumValue("None",  0).
  AddEnumValue("Horizontal Circle", 1).
  AddEnumValue("Vertical Circle",   2).
  AddEnumValue("Bouncing Blocks",   3);
  // Toggle grid
  Menu.AddEnum("Scene Content.Grid Display 'G'",  &GridDisplayMode).AddShortcutKey(Key_G).
  AddEnumValue("No Grid",             GridDisplay_None).
  AddEnumValue("Grid Only",           GridDisplay_GridOnly).
  AddEnumValue("Grid And Scene",      GridDisplay_GridAndScene);
  Menu.AddEnum("Scene Content.Grid Mode 'H'",     &GridMode).AddShortcutKey(Key_H).
  AddEnumValue("4-pixel RT-centered", Grid_Rendertarget4).
  AddEnumValue("16-pixel RT-centered",Grid_Rendertarget16).
  AddEnumValue("Lens-centered grid",  Grid_Lens);
  
  // *** Scene Content Sub-Menu
  Menu.AddBool( "Render Target.Share RenderTarget",  &RendertargetIsSharedByBothEyes).
  AddShortcutKey(Key_F8).SetNotify(this, &OWD::HmdSettingChange);
  Menu.AddBool( "Render Target.Dynamic Res Scaling", &DynamicRezScalingEnabled).
  AddShortcutKey(Key_F8, ShortcutKey::Shift_RequireOn);
  Menu.AddBool( "Render Target.Zero IPD 'F7'",       &ForceZeroIpd).
  AddShortcutKey(Key_F7).
  SetNotify(this, &OWD::HmdSettingChangeFreeRTs);
  Menu.AddFloat("Render Target.Max Fov",             &FovSideTanMax, 0.2f, FovSideTanLimit, 0.02f,
                "%.1f Degrees", 1.0f, &FormatMaxFromSideTan).
  SetNotify(this, &OWD::HmdSettingChange).
  AddShortcutUpKey(Key_I).AddShortcutDownKey(Key_K);
  Menu.AddFloat("Render Target.Pixel Density",    &DesiredPixelDensity, 0.5f, 1.5, 0.025f, "%3.2f", 1.0f).
  SetNotify(this, &OWD::HmdSettingChange);
  Menu.AddEnum( "Render Target.Distortion Clear Color",  &DistortionClearBlue).
  SetNotify(this, &OWD::DistortionClearColorChange).
  AddEnumValue("Black",  0).
  AddEnumValue("Blue", 1);
  
  // Timewarp
  Menu.AddBool( "Timewarp.TimewarpEnabled 'O'",   &TimewarpEnabled).AddShortcutKey(Key_O).
  SetNotify(this, &OWD::HmdSettingChange);
  Menu.AddBool( "Timewarp.FreezeEyeUpdate 'C'",   &FreezeEyeUpdate).AddShortcutKey(Key_C);
  Menu.AddFloat("Timewarp.RenderIntervalSeconds", &TimewarpRenderIntervalInSeconds,
                0.0001f, 1.00f, 0.0001f, "%.1f", 1.0f, &FormatTimewarp).
  AddShortcutUpKey(Key_J).AddShortcutDownKey(Key_U);
  
  // First page properties
  Menu.AddFloat("User Eye Height",    &ThePlayer.UserEyeHeight, 0.2f, 2.5f, 0.02f,
                "%4.2f m").SetNotify(this, &OWD::EyeHeightChange).
  AddShortcutUpKey(Key_Equal).AddShortcutDownKey(Key_Minus);
  Menu.AddFloat("Center Pupil Depth", &CenterPupilDepthMeters, 0.0f, 0.2f, 0.001f,
                "%4.3f m").SetNotify(this, &OWD::CenterPupilDepthChange);
  Menu.AddBool("Body Relative Motion",&ThePlayer.bMotionRelativeToBody).AddShortcutKey(Key_E);
  Menu.AddBool("Zero Head Movement",  &ForceZeroHeadMovement) .AddShortcutKey(Key_F7, ShortcutKey::Shift_RequireOn);
  Menu.AddBool("VSync 'V'",           &VsyncEnabled)          .AddShortcutKey(Key_V).SetNotify(this, &OWD::HmdSettingChange);
  Menu.AddBool("MultiSample 'F4'",    &MultisampleEnabled)    .AddShortcutKey(Key_F4).SetNotify(this, &OWD::MultisampleChange);
  
  // Add DK2 options to menu only for that headset.
  if (HmdDesc.SensorCaps & ovrSensorCap_Position)
  {
    Menu.AddBool("Low Persistence 'P'",      &IsLowPersistence).
    AddShortcutKey(Key_P).SetNotify(this, &OWD::HmdSettingChange);
    Menu.AddBool("Dynamic Prediction",      &DynamicPrediction).
    SetNotify(this, &OWD::HmdSettingChange);
    Menu.AddBool("Positional Tracking 'X'",  &PositionTrackingEnabled).
    AddShortcutKey(Key_X).SetNotify(this, &OWD::HmdSettingChange);
  }
}


void VRApp::CalculateHmdValues()
{
  // Initialize eye rendering information for ovrHmd_Configure.
  // The viewport sizes are re-computed in case RenderTargetSize changed due to HW limitations.
  ovrFovPort eyeFov[2];
  eyeFov[0] = HmdDesc.DefaultEyeFov[0];
  eyeFov[1] = HmdDesc.DefaultEyeFov[1];
  
  // Clamp Fov based on our dynamically adjustable FovSideTanMax.
  // Most apps should use the default, but reducing Fov does reduce rendering cost.
  eyeFov[0] = FovPort::Min(eyeFov[0], FovPort(FovSideTanMax));
  eyeFov[1] = FovPort::Min(eyeFov[1], FovPort(FovSideTanMax));
  
  
  if (ForceZeroIpd)
  {
    // ForceZeroIpd does three things:
    //  1) Sets FOV to maximum symmetrical FOV based on both eyes
    //  2) Sets eye ViewAdjust values to 0.0 (effective IPD == 0)
    //  3) Uses only the Left texture for rendering.
    
    eyeFov[0] = FovPort::Max(eyeFov[0], eyeFov[1]);
    eyeFov[1] = eyeFov[0];
    
    Sizei recommenedTexSize = ovrHmd_GetFovTextureSize(Hmd, ovrEye_Left,
                                                       eyeFov[0], DesiredPixelDensity);
    
    Sizei textureSize = EnsureRendertargetAtLeastThisBig(Rendertarget_Left,  recommenedTexSize);
    
    EyeRenderSize[0] = Sizei::Min(textureSize, recommenedTexSize);
    EyeRenderSize[1] = EyeRenderSize[0];
    
    // Store texture pointers that will be passed for rendering.
    EyeTexture[0]                       = RenderTargets[Rendertarget_Left].Tex;
    EyeTexture[0].Header.TextureSize    = textureSize;
    EyeTexture[0].Header.RenderViewport = Recti(EyeRenderSize[0]);
    // Right eye is the same.
    EyeTexture[1] = EyeTexture[0];
  }
  
  else
  {
    // Configure Stereo settings. Default pixel density is 1.0f.
    Sizei recommenedTex0Size = ovrHmd_GetFovTextureSize(Hmd, ovrEye_Left,  eyeFov[0], DesiredPixelDensity);
    Sizei recommenedTex1Size = ovrHmd_GetFovTextureSize(Hmd, ovrEye_Right, eyeFov[1], DesiredPixelDensity);
    
    if (RendertargetIsSharedByBothEyes)
    {
      Sizei  rtSize(recommenedTex0Size.w + recommenedTex1Size.w,
                    Alg::Max(recommenedTex0Size.h, recommenedTex1Size.h));
      
      // Use returned size as the actual RT size may be different due to HW limits.
      rtSize = EnsureRendertargetAtLeastThisBig(Rendertarget_BothEyes, rtSize);
      
      // Don't draw more then recommended size; this also ensures that resolution reported
      // in the overlay HUD size is updated correctly for FOV/pixel density change.
      EyeRenderSize[0] = Sizei::Min(Sizei(rtSize.w/2, rtSize.h), recommenedTex0Size);
      EyeRenderSize[1] = Sizei::Min(Sizei(rtSize.w/2, rtSize.h), recommenedTex1Size);
      
      // Store texture pointers that will be passed for rendering.
      // Same texture is used, but with different viewports.
      EyeTexture[0]                       = RenderTargets[Rendertarget_BothEyes].Tex;
      EyeTexture[0].Header.TextureSize    = rtSize;
      EyeTexture[0].Header.RenderViewport = Recti(Vector2i(0), EyeRenderSize[0]);
      EyeTexture[1]                       = RenderTargets[Rendertarget_BothEyes].Tex;
      EyeTexture[1].Header.TextureSize    = rtSize;
      EyeTexture[1].Header.RenderViewport = Recti(Vector2i((rtSize.w+1)/2, 0), EyeRenderSize[1]);
    }
    
    else
    {
      Sizei tex0Size = EnsureRendertargetAtLeastThisBig(Rendertarget_Left,  recommenedTex0Size);
      Sizei tex1Size = EnsureRendertargetAtLeastThisBig(Rendertarget_Right, recommenedTex1Size);
      
      EyeRenderSize[0] = Sizei::Min(tex0Size, recommenedTex0Size);
      EyeRenderSize[1] = Sizei::Min(tex1Size, recommenedTex1Size);
      
      // Store texture pointers and viewports that will be passed for rendering.
      EyeTexture[0]                       = RenderTargets[Rendertarget_Left].Tex;
      EyeTexture[0].Header.TextureSize    = tex0Size;
      EyeTexture[0].Header.RenderViewport = Recti(EyeRenderSize[0]);
      EyeTexture[1]                       = RenderTargets[Rendertarget_Right].Tex;
      EyeTexture[1].Header.TextureSize    = tex1Size;
      EyeTexture[1].Header.RenderViewport = Recti(EyeRenderSize[1]);
    }
  }
  
  // Hmd caps.
  unsigned hmdCaps = (VsyncEnabled ? 0 : ovrHmdCap_NoVSync) |
  ovrHmdCap_LatencyTest;
  if (IsLowPersistence)
    hmdCaps |= ovrHmdCap_LowPersistence;
  if (DynamicPrediction)
    hmdCaps |= ovrHmdCap_DynamicPrediction;
  
  ovrHmd_SetEnabledCaps(Hmd, hmdCaps);
  
  
  ovrRenderAPIConfig config         = pRender->Get_ovrRenderAPIConfig();
  unsigned           distortionCaps = ovrDistortionCap_Chromatic;
  if (TimewarpEnabled)
    distortionCaps |= ovrDistortionCap_TimeWarp;
  
  if (!ovrHmd_ConfigureRendering( Hmd, &config, distortionCaps,
                                 eyeFov, EyeRenderDesc ))
  {
    // Fail exit? TBD
    return;
  }
  
  if (ForceZeroIpd)
  {
    // Remove IPD adjust
    EyeRenderDesc[0].ViewAdjust = Vector3f(0);
    EyeRenderDesc[1].ViewAdjust = Vector3f(0);
  }
  
  // ovrHmdCap_LatencyTest - enables internal latency feedback
  unsigned sensorCaps = ovrSensorCap_Orientation|ovrSensorCap_YawCorrection;
  if (PositionTrackingEnabled)
    sensorCaps |= ovrSensorCap_Position;
  
  if (StartSensorCaps != sensorCaps)
  {
    ovrHmd_StartSensor(Hmd, sensorCaps, 0);
    StartSensorCaps = sensorCaps;
  }
  
  // Calculate projections
  Projection[0] = ovrMatrix4f_Projection(EyeRenderDesc[0].Fov,  0.01f, 10000.0f, true);
  Projection[1] = ovrMatrix4f_Projection(EyeRenderDesc[1].Fov,  0.01f, 10000.0f, true);
  
  float    orthoDistance = 0.8f; // 2D is 0.8 meter from camera
  Vector2f orthoScale0   = Vector2f(1.0f) / Vector2f(EyeRenderDesc[0].PixelsPerTanAngleAtCenter);
  Vector2f orthoScale1   = Vector2f(1.0f) / Vector2f(EyeRenderDesc[1].PixelsPerTanAngleAtCenter);
  
  OrthoProjection[0] = ovrMatrix4f_OrthoSubProjection(Projection[0], orthoScale0, orthoDistance,
                                                      EyeRenderDesc[0].ViewAdjust.x);
  OrthoProjection[1] = ovrMatrix4f_OrthoSubProjection(Projection[1], orthoScale1, orthoDistance,
                                                      EyeRenderDesc[1].ViewAdjust.x);
}



// Returns the actual size present.
Sizei VRApp::EnsureRendertargetAtLeastThisBig(int rtNum, Sizei requestedSize)
{
  OVR_ASSERT((rtNum >= 0) && (rtNum < Rendertarget_LAST));
  
  // Texture size that we already have might be big enough.
  Sizei newRTSize;
  
  RenderTarget& rt = RenderTargets[rtNum];
  if (!rt.pTex)
  {
    // Hmmm... someone nuked my texture. Rez change or similar. Make sure we reallocate.
    rt.Tex.Header.TextureSize = Sizei(0);
    newRTSize = requestedSize;
  }
  else
  {
    newRTSize = rt.Tex.Header.TextureSize;
  }
  
  // %50 linear growth each time is a nice balance between being too greedy
  // for a 2D surface and too slow to prevent fragmentation.
  while ( newRTSize.w < requestedSize.w )
  {
    newRTSize.w += newRTSize.w/2;
  }
  while ( newRTSize.h < requestedSize.h )
  {
    newRTSize.h += newRTSize.h/2;
  }
  
  // Put some sane limits on it. 4k x 4k is fine for most modern video cards.
  // Nobody should be messing around with surfaces smaller than 4k pixels these days.
  newRTSize = Sizei::Max(Sizei::Min(newRTSize, Sizei(4096)), Sizei(64));
  
  // Does that require actual reallocation?
  if (Sizei(rt.Tex.Header.TextureSize) != newRTSize)
  {
    rt.pTex = *pRender->CreateTexture(Texture_RGBA | Texture_RenderTarget | (MultisampleEnabled ? 4 : 1),
                                      newRTSize.w, newRTSize.h, NULL);
    rt.pTex->SetSampleMode(Sample_ClampBorder | Sample_Linear);
    
    
    // Configure texture for SDK Rendering.
    rt.Tex = rt.pTex->Get_ovrTexture();
  }
  
  return newRTSize;
}


//-----------------------------------------------------------------------------
// ***** Message Handlers

void VRApp::OnResize(int width, int height)
{
  WindowSize = Sizei(width, height);
  HmdSettingsChanged = true;
}

void VRApp::OnMouseMove(int x, int y, int modifiers)
{
  OVR_UNUSED(y);
  if(modifiers & Mod_MouseRelative)
  {
    // Get Delta
    int dx = x;
    
    // Apply to rotation. Subtract for right body frame rotation,
    // since yaw rotation is positive CCW when looking down on XZ plane.
    ThePlayer.BodyYaw   -= (Sensitivity * dx) / 360.0f;
  }
}


void VRApp::OnKey(OVR::KeyCode key, int chr, bool down, int modifiers)
{
  if (Menu.OnKey(key, chr, down, modifiers))
    return;
  
  // Handle player movement keys.
  if (ThePlayer.HandleMoveKey(key, down))
    return;
  
  switch(key)
  {
    case Key_Q:
      if (down && (modifiers & Mod_Control))
        pPlatform->Exit(0);
      break;
      
    case Key_Escape:
      // Back to primary windowed
      if (!down) exit(0); // ChangeDisplay ( true, false, false );
      break;
      
    case Key_F9:
      // Cycle through displays, going fullscreen on each one.
      if (!down) ChangeDisplay ( false, true, false );
      break;
      
#ifdef OVR_OS_MAC
      // F11 is reserved on Mac, F10 doesn't work on Windows
    case Key_F10:
#else
    case Key_F11:
#endif
      if (!down) ChangeDisplay ( false, false, true );
      break;
      
    case Key_R:
      if (!down)
      {
        ovrHmd_ResetSensor(Hmd);
        Menu.SetPopupMessage("Sensor Fusion Reset");
      }
      break;
      
    case Key_Space:
      if (!down)
      {
        TextScreen = (enum TextScreen)((TextScreen + 1) % Text_Count);
      }
      break;
      
      // Distortion correction adjustments
    case Key_Backslash:
      break;
      // Holding down Shift key accelerates adjustment velocity.
    case Key_Shift:
      ShiftDown = down;
      break;
    case Key_Control:
      CtrlDown = down;
      break;
      
      // Reset the camera position in case we get stuck
    case Key_T:
      if (down)
      {
        struct {
          float  x, z;
          float  YawDegrees;
        }  Positions[] =
        {
          // x         z           Yaw
          { 7.7f,     -1.0f,      180.0f },   // The starting position.
          { 10.0f,    10.0f,      90.0f  },   // Outside, looking at some trees.
          { 19.26f,   5.43f,      22.0f  },   // Outside, looking at the fountain.
        };
        
        static int nextPosition = 0;
        nextPosition = (nextPosition + 1) % (sizeof(Positions)/sizeof(Positions[0]));
        
        ThePlayer.BodyPos = Vector3f(Positions[nextPosition].x,
                                     ThePlayer.UserEyeHeight, Positions[nextPosition].z);
        ThePlayer.BodyYaw = DegreeToRad( Positions[nextPosition].YawDegrees );
      }
      break;
      
    case Key_Num1:
      ThePlayer.BodyPos = Vector3f(-1.85f, 6.0f, -0.52f);
      ThePlayer.BodyPos.y += ThePlayer.UserEyeHeight;
      ThePlayer.BodyYaw = 3.1415f / 2;
      ThePlayer.HandleMovement(0, &CollisionModels, &GroundCollisionModels, ShiftDown);
      break;
      
    default:
      break;
  }
}

//-----------------------------------------------------------------------------


Matrix4f VRApp::CalculateViewFromPose(const Transformf& pose)
{
  Transformf worldPose = ThePlayer.VirtualWorldTransformfromRealPose(pose);
  
  // Rotate and position View Camera
  Vector3f up      = worldPose.Rotation.Rotate(UpVector);
  Vector3f forward = worldPose.Rotation.Rotate(ForwardVector);
  
  // Transform the position of the center eye in the real world (i.e. sitting in your chair)
  // into the frame of the player's virtual body.
  
  // It is important to have head movement in scale with IPD.
  // If you shrink one, you should also shrink the other.
  // So with zero IPD (i.e. everything at infinity),
  // head movement should also be zero.
  Vector3f viewPos = ForceZeroHeadMovement ? ThePlayer.BodyPos : worldPose.Translation;
  
  Matrix4f view = Matrix4f::LookAtRH(viewPos, viewPos + forward, up);
  return view;
}



void VRApp::OnIdle()
{
  double curtime = ovr_GetTimeInSeconds();
  // If running slower than 10fps, clamp. Helps when debugging, because then dt can be minutes!
  float  dt      = Alg::Min<float>(float(curtime - LastUpdate), 0.1f);
  LastUpdate     = curtime;
  
  
  Profiler.RecordSample(RenderProfiler::Sample_FrameStart);
  
  if (LoadingState == LoadingState_DoLoad)
  {
    //PopulateScene(MainFilePath.ToCStr());
    Game->LoadNextLevel(MainScene); // TODO: Replace with Game->LoadMenu
    LoadingState = LoadingState_Finished;
    return;
  } else if (Game->CurrentLevel()->LevelComplete()) {
    Game->LoadNextLevel(MainScene);
  }
  
  if (HmdSettingsChanged)
  {
    CalculateHmdValues();
    HmdSettingsChanged = false;
  }
  
  HmdFrameTiming = ovrHmd_BeginFrame(Hmd, 0);
  
  
  // Update gamepad.
  GamepadState gamepadState;
  if (GetPlatformCore()->GetGamepadManager()->GetGamepadState(0, &gamepadState))
  {
    GamepadStateChanged(gamepadState);
  }
  
  SensorState ss = ovrHmd_GetSensorState(Hmd, HmdFrameTiming.ScanoutMidpointSeconds);
  HmdStatus = ss.StatusFlags;
  
  // Change message status around positional tracking.
  bool hadVisionTracking = HaveVisionTracking;
  HaveVisionTracking = (ss.StatusFlags & Status_PositionTracked) != 0;
  if (HaveVisionTracking && !hadVisionTracking)
    Menu.SetPopupMessage("Vision Tracking Acquired");
  if (!HaveVisionTracking && hadVisionTracking)
    Menu.SetPopupMessage("Lost Vision Tracking");
  
  // Check if any new devices were connected.
  ProcessDeviceNotificationQueue();
  // FPS count and timing.
  UpdateFrameRateCounter(curtime);
  
  
  // Update pose based on frame!
  ThePlayer.HeadPose = ss.Predicted.Pose;
  // Movement/rotation with the gamepad.
  ThePlayer.BodyYaw -= ThePlayer.GamepadRotate.x * dt;
  ThePlayer.HandleMovement(dt, &CollisionModels, &GroundCollisionModels, ShiftDown);
  
  
  // Record after processing time.
  Profiler.RecordSample(RenderProfiler::Sample_AfterGameProcessing);
  
  
  // Determine if we are rendering this frame. Frame rendering may be
  // skipped based on FreezeEyeUpdate and Time-warp timing state.
  bool bupdateRenderedView = FrameNeedsRendering(curtime);
  
  if (bupdateRenderedView)
  {
    // If render texture size is changing, apply dynamic changes to viewport.
    ApplyDynamicResolutionScaling();
    
    pRender->BeginScene(PostProcess_None);
    
    if (ForceZeroIpd)
    {
      // Zero IPD eye rendering: draw into left eye only,
      // re-use  texture for right eye.
      pRender->SetRenderTarget(RenderTargets[Rendertarget_Left].pTex);
      pRender->Clear();
      
      ovrPosef eyeRenderPose = ovrHmd_BeginEyeRender(Hmd, ovrEye_Left);
      
      View = CalculateViewFromPose(eyeRenderPose);
      RenderEyeView(ovrEye_Left);
      ovrHmd_EndEyeRender(Hmd, ovrEye_Left, eyeRenderPose, &EyeTexture[ovrEye_Left]);
      
      // Second eye gets the same texture (initialized to same value above).
      ovrHmd_BeginEyeRender(Hmd, ovrEye_Right);
      ovrHmd_EndEyeRender(Hmd, ovrEye_Right, eyeRenderPose, &EyeTexture[ovrEye_Right]);
    }
    
    else if (RendertargetIsSharedByBothEyes)
    {
      // Shared render target eye rendering; set up RT once for both eyes.
      pRender->SetRenderTarget(RenderTargets[Rendertarget_BothEyes].pTex);
      pRender->Clear();
      
      for (int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++)
      {
        ovrEyeType eye = HmdDesc.EyeRenderOrder[eyeIndex];
        ovrPosef eyeRenderPose = ovrHmd_BeginEyeRender(Hmd, eye);
        
        View = CalculateViewFromPose(eyeRenderPose);
        RenderEyeView(eye);
        ovrHmd_EndEyeRender(Hmd, eye, eyeRenderPose, &EyeTexture[eye]);
      }
    }
    
    else
    {
      // Separate eye rendering - each eye gets its own render target.
      for (int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++)
      {
        ovrEyeType eye = HmdDesc.EyeRenderOrder[eyeIndex];
        pRender->SetRenderTarget(
                                 RenderTargets[(eye == 0) ? Rendertarget_Left : Rendertarget_Right].pTex);
        pRender->Clear();
        
        ovrPosef eyeRenderPose = ovrHmd_BeginEyeRender(Hmd, eye);
        
        View = CalculateViewFromPose(eyeRenderPose);
        RenderEyeView(eye);
        ovrHmd_EndEyeRender(Hmd, eye, eyeRenderPose, &EyeTexture[eye]);
      }
    }
    
    pRender->SetDefaultRenderTarget();
    pRender->FinishScene();
  }
  
  /*
   double t= ovr_GetTimeInSeconds();
   while (ovr_GetTimeInSeconds() < (t + 0.017))
   {
   
   } */
  
  Profiler.RecordSample(RenderProfiler::Sample_AfterEyeRender);
  
  // TODO: These happen inside ovrHmd_EndFrame; need to hook into it.
  //Profiler.RecordSample(RenderProfiler::Sample_BeforeDistortion);
  ovrHmd_EndFrame(Hmd);
  Profiler.RecordSample(RenderProfiler::Sample_AfterPresent);
}



// Determine whether this frame needs rendering based on time-warp timing and flags.
bool VRApp::FrameNeedsRendering(double curtime)
{
  static double   lastUpdate          = 0.0;
  double          renderInterval      = TimewarpRenderIntervalInSeconds;
  double          timeSinceLast       = curtime - lastUpdate;
  bool            updateRenderedView  = true;
  
  if (TimewarpEnabled)
  {
    if (FreezeEyeUpdate)
    {
      // Draw one frame after (FreezeEyeUpdate = true) to update message text.
      if (FreezeEyeOneFrameRendered)
        updateRenderedView = false;
      else
        FreezeEyeOneFrameRendered = true;
    }
    else
    {
      FreezeEyeOneFrameRendered = false;
      
      if ( (timeSinceLast < 0.0) || ((float)timeSinceLast > renderInterval) )
      {
        // This allows us to do "fractional" speeds, e.g. 45fps rendering on a 60fps display.
        lastUpdate += renderInterval;
        if ( timeSinceLast > 5.0 )
        {
          // renderInterval is probably tiny (i.e. "as fast as possible")
          lastUpdate = curtime;
        }
        
        updateRenderedView = true;
      }
      else
      {
        updateRenderedView = false;
      }
    }
  }
  
  return updateRenderedView;
}


void VRApp::ApplyDynamicResolutionScaling()
{
  if (!DynamicRezScalingEnabled)
  {
    // Restore viewport rectangle in case dynamic res scaling was enabled before.
    EyeTexture[0].Header.RenderViewport.Size = EyeRenderSize[0];
    EyeTexture[1].Header.RenderViewport.Size = EyeRenderSize[1];
    return;
  }
  
  // Demonstrate dynamic-resolution rendering.
  // This demo is too simple to actually have a framerate that varies that much, so we'll
  // just pretend this is trying to cope with highly dynamic rendering load.
  float dynamicRezScale = 1.0f;
  
  {
    // Hacky stuff to make up a scaling...
    // This produces value oscillating as follows: 0 -> 1 -> 0.
    static double dynamicRezStartTime   = ovr_GetTimeInSeconds();
    float         dynamicRezPhase       = float ( ovr_GetTimeInSeconds() - dynamicRezStartTime );
    const float   dynamicRezTimeScale   = 4.0f;
    
    dynamicRezPhase /= dynamicRezTimeScale;
    if ( dynamicRezPhase < 1.0f )
    {
      dynamicRezScale = dynamicRezPhase;
    }
    else if ( dynamicRezPhase < 2.0f )
    {
      dynamicRezScale = 2.0f - dynamicRezPhase;
    }
    else
    {
      // Reset it to prevent creep.
      dynamicRezStartTime = ovr_GetTimeInSeconds();
      dynamicRezScale     = 0.0f;
    }
    
    // Map oscillation: 0.5 -> 1.0 -> 0.5
    dynamicRezScale = dynamicRezScale * 0.5f + 0.5f;
  }
  
  Sizei sizeLeft  = EyeRenderSize[0];
  Sizei sizeRight = EyeRenderSize[1];
  
  // This viewport is used for rendering and passed into ovrHmd_EndEyeRender.
  EyeTexture[0].Header.RenderViewport.Size = Sizei(int(sizeLeft.w  * dynamicRezScale),
                                                   int(sizeLeft.h  * dynamicRezScale));
  EyeTexture[1].Header.RenderViewport.Size = Sizei(int(sizeRight.w * dynamicRezScale),
                                                   int(sizeRight.h * dynamicRezScale));
}


void VRApp::UpdateFrameRateCounter(double curtime)
{
  FrameCounter++;
  float secondsSinceLastMeasurement = (float)( curtime - LastFpsUpdate );
  
  if (secondsSinceLastMeasurement >= SecondsOfFpsMeasurement)
  {
    SecondsPerFrame = (float)( curtime - LastFpsUpdate ) / (float)FrameCounter;
    FPS             = 1.0f / SecondsPerFrame;
    LastFpsUpdate   = curtime;
    FrameCounter =   0;
  }
}


void VRApp::RenderEyeView(ovrEyeType eye)
{
  Recti    renderViewport = EyeTexture[eye].Header.RenderViewport;
  Matrix4f viewAdjust     = Matrix4f::Translation(Vector3f(EyeRenderDesc[eye].ViewAdjust));
  
  
  // *** 3D - Configures Viewport/Projection and Render
  
  pRender->ApplyStereoParams(renderViewport, Projection[eye]);
  pRender->SetDepthMode(true, true);
  
  Matrix4f baseTranslate = Matrix4f::Translation(ThePlayer.BodyPos);
  Matrix4f baseYaw       = Matrix4f::RotationY(ThePlayer.BodyYaw.Get());
  
  
  if (GridDisplayMode != GridDisplay_GridOnly)
  {
    if (SceneMode != Scene_OculusCubes)
    {
      MainScene.Render(pRender, viewAdjust * View);
      RenderAnimatedBlocks(eye, ovr_GetTimeInSeconds());
    }
    
    if (SceneMode == Scene_Cubes)
    {
      // Draw scene cubes overlay. Red if position tracked, blue otherwise.
      Scene sceneCubes = (HmdStatus & ovrStatus_PositionTracked) ?
      RedCubesScene : BlueCubesScene;
      sceneCubes.Render(pRender, viewAdjust * View * baseTranslate * baseYaw);
    }
    
    else if (SceneMode == Scene_OculusCubes)
    {
      OculusCubesScene.Render(pRender, viewAdjust * View * baseTranslate * baseYaw);
    }
  }
  
  if (GridDisplayMode != GridDisplay_None)
  {
    RenderGrid(eye);
  }
  
  
  // *** 2D Text - Configure Orthographic rendering.
  
  // Render UI in 2D orthographic coordinate system that maps [-1,1] range
  // to a readable FOV area centered at your eye and properly adjusted.
  pRender->ApplyStereoParams(renderViewport, OrthoProjection[eye]);
  pRender->SetDepthMode(false, false);
  
  // We set this scale up in CreateOrthoSubProjection().
  float textHeight = 22.0f;
  
  // Display Loading screen-shot in frame 0.
  if (LoadingState != LoadingState_Finished)
  {
    const float scale = textHeight * 25.0f;
    Matrix4f view ( scale, 0.0f, 0.0f, 0.0f, scale, 0.0f, 0.0f, 0.0f, scale );
    LoadingScene.Render(pRender, view);
    String loadMessage = String("Loading ") + MainFilePath;
    DrawTextBox(pRender, 0.0f, -textHeight, textHeight, loadMessage.ToCStr(), DrawText_HCenter);
    LoadingState = LoadingState_DoLoad;
  }
  
  // HUD overlay brought up by spacebar.
  RenderTextInfoHud(textHeight);
  
  // Menu brought up by
  Menu.Render(pRender);
}



// NOTE - try to keep these in sync with the PDF docs!
static const char* HelpText1 =
"Spacebar               \t500 Toggle debug info overlay\n"
"W, S                  \t500 Move forward, back\n"
"A, D               \t500 Strafe left, right\n"
"Mouse move           \t500 Look left, right\n"
"Left gamepad stick     \t500 Move\n"
"Right gamepad stick    \t500 Turn\n"
"T                  \t500 Reset player position";

static const char* HelpText2 =
"R              \t250 Reset sensor orientation\n"
"G           \t250 Cycle grid overlay mode\n"
"-, +           \t250 Adjust eye height\n"
"Esc            \t250 Cancel full-screen\n"
"F4          \t250 Multisampling toggle\n"
"F9             \t250 Hardware full-screen (low latency)\n"
"F11            \t250 Faked full-screen (easier debugging)\n"
"Ctrl+Q        \t250 Quit";


void FormatLatencyReading(char* buff, UPInt size, float val)
{
  if (val < 0.000001f)
    OVR_strcpy(buff, size, "N/A   ");
  else
    OVR_sprintf(buff, size, "%4.2fms", val * 1000.0f);
}


void VRApp::RenderTextInfoHud(float textHeight)
{
  // View port & 2D ortho projection must be set before call.
  
  float hmdYaw, hmdPitch, hmdRoll;
  switch(TextScreen)
  {
    case Text_Info:
    {
      char buf[512], gpustat[256];
      
      // Average FOVs.
      FovPort leftFov  = EyeRenderDesc[0].Fov;
      FovPort rightFov = EyeRenderDesc[1].Fov;
      
      // Rendered size changes based on selected options & dynamic rendering.
      int pixelSizeWidth = EyeTexture[0].Header.RenderViewport.Size.w +
      ((!ForceZeroIpd) ?
       EyeTexture[1].Header.RenderViewport.Size.w : 0);
      int pixelSizeHeight = ( EyeTexture[0].Header.RenderViewport.Size.h +
                             EyeTexture[1].Header.RenderViewport.Size.h ) / 2;
      
      // No DK2, no message.
      char latency2Text[128] = "";
      {
        //float latency2 = ovrHmd_GetMeasuredLatencyTest2(Hmd) * 1000.0f; // show it in ms
        //if (latency2 > 0)
        //    OVR_sprintf(latency2Text, sizeof(latency2Text), "%.2fms", latency2);
        
        float latencies[3] = { 0.0f, 0.0f, 0.0f };
        if (ovrHmd_GetFloatArray(Hmd, "DK2Latency", latencies, 3) == 3)
        {
          char latencyText0[32], latencyText1[32], latencyText2[32];
          FormatLatencyReading(latencyText0, sizeof(latencyText0), latencies[0]);
          FormatLatencyReading(latencyText1, sizeof(latencyText1), latencies[1]);
          FormatLatencyReading(latencyText2, sizeof(latencyText2), latencies[2]);
          
          OVR_sprintf(latency2Text, sizeof(latency2Text),
                      " DK2 Latency  Ren: %s  TWrp: %s\n"
                      " PostPresent: %s  ",
                      latencyText0, latencyText1, latencyText2);
        }
      }
      
      ThePlayer.HeadPose.Rotation.GetEulerAngles<Axis_Y, Axis_X, Axis_Z>(&hmdYaw, &hmdPitch, &hmdRoll);
      OVR_sprintf(buf, sizeof(buf),
                  " HMD YPR:%4.0f %4.0f %4.0f   Player Yaw: %4.0f\n"
                  " FPS: %.1f  ms/frame: %.1f Frame: %d\n"
                  " Pos: %3.2f, %3.2f, %3.2f  HMD: %s\n"
                  " EyeHeight: %3.2f, IPD: %3.1fmm\n" //", Lens: %s\n"
                  " FOV %3.1fx%3.1f, Resolution: %ix%i\n"
                  "%s",
                  RadToDegree(hmdYaw), RadToDegree(hmdPitch), RadToDegree(hmdRoll),
                  RadToDegree(ThePlayer.BodyYaw.Get()),
                  FPS, SecondsPerFrame * 1000.0f, FrameCounter,
                  ThePlayer.BodyPos.x, ThePlayer.BodyPos.y, ThePlayer.BodyPos.z,
                  //GetDebugNameHmdType ( TheHmdRenderInfo.HmdType ),
                  HmdDesc.ProductName,
                  ThePlayer.UserEyeHeight,
                  ovrHmd_GetFloat(Hmd, OVR_KEY_IPD, 0) * 1000.0f,
                  //( EyeOffsetFromNoseLeft + EyeOffsetFromNoseRight ) * 1000.0f,
                  //GetDebugNameEyeCupType ( TheHmdRenderInfo.EyeCups ),  // Lens/EyeCup not exposed
                  
                  (leftFov.GetHorizontalFovDegrees() + rightFov.GetHorizontalFovDegrees()) * 0.5f,
                  (leftFov.GetVerticalFovDegrees() + rightFov.GetVerticalFovDegrees()) * 0.5f,
                  
                  pixelSizeWidth, pixelSizeHeight,
                  
                  latency2Text
                  );
      
      size_t texMemInMB = pRender->GetTotalTextureMemoryUsage() / 1058576;
      if (texMemInMB)
      {
        OVR_sprintf(gpustat, sizeof(gpustat), " GPU Tex: %u MB", texMemInMB);
        OVR_strcat(buf, sizeof(buf), gpustat);
      }
      
      DrawTextBox(pRender, 0.0f, 0.0f, textHeight, buf, DrawText_Center);
    }
      break;
      
    case Text_Timing:
      Profiler.DrawOverlay(pRender);
      break;
      
    case Text_Help1:
      DrawTextBox(pRender, 0.0f, 0.0f, textHeight, HelpText1, DrawText_Center);
      break;
    case Text_Help2:
      DrawTextBox(pRender, 0.0f, 0.0f, textHeight, HelpText2, DrawText_Center);
      break;
      
    case Text_None:
      break;
      
    default:
      OVR_ASSERT ( !"Missing text screen" );
      break;
  }
}


//-----------------------------------------------------------------------------
// ***** Callbacks For Menu changes

// Non-trivial callback go here.

void VRApp::HmdSettingChangeFreeRTs(OptionVar*)
{
  HmdSettingsChanged = true;
  // Cause the RTs to be recreated with the new mode.
  for ( int rtNum = 0; rtNum < Rendertarget_LAST; rtNum++ )
    RenderTargets[rtNum].pTex = NULL;
}

void VRApp::MultisampleChange(OptionVar*)
{
  HmdSettingChangeFreeRTs();
}

void VRApp::CenterPupilDepthChange(OptionVar*)
{
  ovrHmd_SetFloat(Hmd, "CenterPupilDepth", CenterPupilDepthMeters);
}

void VRApp::DistortionClearColorChange(OptionVar*)
{
  float clearColor[2][4] = { { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.5f, 1.0f, 0.0f } };
  ovrHmd_SetFloatArray(Hmd, "DistortionClearColor",
                       clearColor[(int)DistortionClearBlue], 4);
}


//-----------------------------------------------------------------------------

void VRApp::ProcessDeviceNotificationQueue()
{
  // TBD: Process device plug & Unplug
}


//-----------------------------------------------------------------------------
void VRApp::ChangeDisplay ( bool bBackToWindowed, bool bNextFullscreen,
                                        bool bFullWindowDebugging )
{
  // Exactly one should be set...
  OVR_ASSERT ( ( bBackToWindowed ? 1 : 0 ) + ( bNextFullscreen ? 1 : 0 ) +
              ( bFullWindowDebugging ? 1 : 0 ) == 1 );
  OVR_UNUSED ( bNextFullscreen );
  
  if ( bFullWindowDebugging )
  {
    // Slightly hacky. Doesn't actually go fullscreen, just makes a screen-sized wndow.
    // This has higher latency than fullscreen, and is not intended for actual use,
    // but makes for much nicer debugging on some systems.
    RenderParams = pRender->GetParams();
    RenderParams.Display = DisplayId(HmdDesc.DisplayDeviceName, HmdDesc.DisplayId);
    pRender->SetParams(RenderParams);
    
    pPlatform->SetMouseMode(Mouse_Normal);
    pPlatform->SetFullscreen(RenderParams, pRender->IsFullscreen() ? Display_Window : Display_FakeFullscreen);
    pPlatform->SetMouseMode(Mouse_Relative); // Avoid mode world rotation jump.
    
    // If using an HMD, enable post-process (for distortion) and stereo.
    if (RenderParams.IsDisplaySet() && pRender->IsFullscreen())
    {
      //SetPostProcessingMode ( PostProcess );
    }
  }
  else
  {
    int screenCount = pPlatform->GetDisplayCount();
    
    int screenNumberToSwitchTo;
    if ( bBackToWindowed )
    {
      screenNumberToSwitchTo = -1;
    }
    else
    {
      if (!pRender->IsFullscreen())
      {
        // Currently windowed.
        // Try to find HMD Screen, making it the first screen in the full-screen cycle.
        FirstScreenInCycle = 0;
        if (!UsingDebugHmd)
        {
          DisplayId HMD (HmdDesc.DisplayDeviceName, HmdDesc.DisplayId);
          for (int i = 0; i< screenCount; i++)
          {
            if (pPlatform->GetDisplay(i) == HMD)
            {
              FirstScreenInCycle = i;
              break;
            }
          }
        }
        ScreenNumber = FirstScreenInCycle;
        screenNumberToSwitchTo = ScreenNumber;
      }
      else
      {
        // Currently fullscreen, so cycle to the next screen.
        ScreenNumber++;
        if (ScreenNumber == screenCount)
        {
          ScreenNumber = 0;
        }
        screenNumberToSwitchTo = ScreenNumber;
        if (ScreenNumber == FirstScreenInCycle)
        {
          // We have cycled through all the fullscreen displays, so go back to windowed mode.
          screenNumberToSwitchTo = -1;
        }
      }
    }
    
    // Always restore windowed mode before going to next screen, even if we were already fullscreen.
    pPlatform->SetFullscreen(RenderParams, Display_Window);
    if ( screenNumberToSwitchTo >= 0 )
    {
      // Go fullscreen.
      RenderParams.Display = pPlatform->GetDisplay(screenNumberToSwitchTo);
      pRender->SetParams(RenderParams);
      pPlatform->SetFullscreen(RenderParams, Display_Fullscreen);
    }
  }
  
  
  // Updates render target pointers & sizes.
  HmdSettingChangeFreeRTs();
}

void VRApp::GamepadStateChanged(const GamepadState& pad)
{
  ThePlayer.GamepadMove   = Vector3f(pad.LX * pad.LX * (pad.LX > 0 ? 1 : -1),
                                     0,
                                     pad.LY * pad.LY * (pad.LY > 0 ? -1 : 1));
  ThePlayer.GamepadRotate = Vector3f(2 * pad.RX, -2 * pad.RY, 0);
  
  UInt32 gamepadDeltas = (pad.Buttons ^ LastGamepadState.Buttons) & pad.Buttons;
  
  if (gamepadDeltas)
  {
    Menu.OnGamepad(gamepadDeltas);
  }
  
  LastGamepadState = pad;
}

// Loads the scene data
void VRApp::PopulateScene(const char *fileName)
{
  XmlHandler xmlHandler;
  if(!xmlHandler.ReadFile(fileName, pRender, &MainScene, &CollisionModels, &GroundCollisionModels))
  {
    Menu.SetPopupMessage("FILE LOAD FAILED");
    Menu.SetPopupTimeout(10.0f, true);
  }
  
  MainScene.SetAmbient(Color4f(1.0f, 1.0f, 1.0f, 1.0f));  
}


//-------------------------------------------------------------------------------------
// ***** Scene Creation / Loading

void VRApp::InitMainFilePath()
{
  
  MainFilePath = WORLDDEMO_ASSET_FILE;
  
  // Try to modify path for correctness in case specified file is not found.
  if (!SysFile(MainFilePath).IsValid())
  {
    String prefixPath1(pPlatform->GetContentDirectory() + "/" + WORLDDEMO_ASSET_PATH1),
    prefixPath2(WORLDDEMO_ASSET_PATH2),
    prefixPath3(WORLDDEMO_ASSET_PATH3);
    if (SysFile(prefixPath1 + MainFilePath).IsValid())
      MainFilePath = prefixPath1 + MainFilePath;
    else if (SysFile(prefixPath2 + MainFilePath).IsValid())
      MainFilePath = prefixPath2 + MainFilePath;
    else if (SysFile(prefixPath3 + MainFilePath).IsValid())
      MainFilePath = prefixPath3 + MainFilePath;
  }
}




void VRApp::PopulatePreloadScene()
{
  // Load-screen screen shot image
  String fileName = MainFilePath;
  fileName.StripExtension();
  
  Ptr<File>    imageFile = *new SysFile(fileName + "_LoadScreen.tga");
  Ptr<Texture> imageTex;
  if (imageFile->IsValid())
    imageTex = *LoadTextureTga(pRender, imageFile);
  
  // Image is rendered as a single quad.
  if (imageTex)
  {
    imageTex->SetSampleMode(Sample_Anisotropic|Sample_Repeat);
    Ptr<Model> m = *new Model(Prim_Triangles);
    m->AddVertex(-0.5f,  0.5f,  0.0f, Color(255,255,255,255), 0.0f, 0.0f);
    m->AddVertex( 0.5f,  0.5f,  0.0f, Color(255,255,255,255), 1.0f, 0.0f);
    m->AddVertex( 0.5f, -0.5f,  0.0f, Color(255,255,255,255), 1.0f, 1.0f);
    m->AddVertex(-0.5f, -0.5f,  0.0f, Color(255,255,255,255), 0.0f, 1.0f);
    m->AddTriangle(2,1,0);
    m->AddTriangle(0,3,2);
    
    Ptr<ShaderFill> fill = *new ShaderFill(*pRender->CreateShaderSet());
    fill->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Vertex, VShader_MVP));
    fill->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Fragment, FShader_Texture));
    fill->SetTexture(0, imageTex);
    m->Fill = fill;
    
    LoadingScene.World.Add(m);
  }
}

void VRApp::ClearScene()
{
  MainScene.Clear();
  SmallGreenCube.Clear();
}


//-------------------------------------------------------------------------------------
// ***** Rendering Content


void VRApp::RenderAnimatedBlocks(ovrEyeType eye, double appTime)
{
  Matrix4f viewAdjust = Matrix4f::Translation(Vector3f(EyeRenderDesc[eye].ViewAdjust));
  
  switch ( BlocksShowType )
  {
    case 0:
      // No blocks;
      break;
    case 1: {
      // Horizontal circle around your head.
      int const   numBlocks = 10;
      float const radius = 1.0f;
      Matrix4f    scaleUp = Matrix4f::Scaling ( 20.0f );
      double      scaledTime = appTime * 0.1;
      float       fracTime = (float)( scaledTime - floor ( scaledTime ) );
      
      for ( int j = 0; j < 2; j++ )
      {
        for ( int i = 0; i < numBlocks; i++ )
        {
          float angle = ( ( (float)i / numBlocks ) + fracTime ) * ( Math<float>::Pi * 2.0f );
          Vector3f pos;
          pos.x = BlocksCenter.x + radius * cosf ( angle );
          pos.y = BlocksCenter.y;
          pos.z = BlocksCenter.z + radius * sinf ( angle );
          if ( j == 0 )
          {
            pos.x = BlocksCenter.x - radius * cosf ( angle );
            pos.y = BlocksCenter.y - 0.5f;
          }
          Matrix4f mat = Matrix4f::Translation ( pos );
          SmallGreenCube.Render(pRender, viewAdjust * View * mat * scaleUp);
        }
      }
    }break;
      
    case 2: {
      // Vertical circle around your head.
      int const   numBlocks = 10;
      float const radius = 1.0f;
      Matrix4f    scaleUp = Matrix4f::Scaling ( 20.0f );
      double      scaledTime = appTime * 0.1;
      float       fracTime = (float)( scaledTime - floor ( scaledTime ) );
      
      for ( int j = 0; j < 2; j++ )
      {
        for ( int i = 0; i < numBlocks; i++ )
        {
          float angle = ( ( (float)i / numBlocks ) + fracTime ) * ( Math<float>::Pi * 2.0f );
          Vector3f pos;
          pos.x = BlocksCenter.x;
          pos.y = BlocksCenter.y + radius * cosf ( angle );
          pos.z = BlocksCenter.z + radius * sinf ( angle );
          if ( j == 0 )
          {
            pos.x = BlocksCenter.x - 0.5f;
            pos.y = BlocksCenter.y - radius * cosf ( angle );
          }
          Matrix4f mat = Matrix4f::Translation ( pos );
          SmallGreenCube.Render(pRender, viewAdjust * View * mat * scaleUp);
        }
      }
    }break;
      
    case 3:{
      // Bouncing.
      int const   numBlocks = 10;
      Matrix4f    scaleUp = Matrix4f::Scaling ( 20.0f );
      
      for ( int i = 0; i < numBlocks; i++ )
      {
        double scaledTime = 4.0f * appTime / (double)i;
        float fracTime = (float)( scaledTime - floor ( scaledTime ) );
        
        Vector3f pos = BlocksCenter;
        pos.z += (float)i;
        pos.y += -1.5f + 4.0f * ( 2.0f * fracTime * ( 1.0f - fracTime ) );
        Matrix4f mat = Matrix4f::Translation ( pos );
        SmallGreenCube.Render(pRender, viewAdjust * View * mat * scaleUp);
      }
    }break;
      
    default:
      BlocksShowType = 0;
      break;
  }
}

void VRApp::RenderGrid(ovrEyeType eye)
{
  Recti renderViewport = EyeTexture[eye].Header.RenderViewport;
  
  // Draw actual pixel grid on the RT.
  // 1:1 mapping to screen pixels, origin in top-left.
  Matrix4f ortho;
  ortho.SetIdentity();
  ortho.M[0][0] = 2.0f / (renderViewport.w);       // X scale
  ortho.M[0][3] = -1.0f;                           // X offset
  ortho.M[1][1] = -2.0f / (renderViewport.h);      // Y scale (for Y=down)
  ortho.M[1][3] = 1.0f;                            // Y offset (Y=down)
  ortho.M[2][2] = 0;
  pRender->SetProjection(ortho);
  
  pRender->SetDepthMode(false, false);
  Color cNormal ( 255, 0, 0 );
  Color cSpacer ( 255, 255, 0 );
  Color cMid ( 0, 128, 255 );
  
  int lineStep = 1;
  int midX = 0;
  int midY = 0;
  int limitX = 0;
  int limitY = 0;
  
  switch ( GridMode )
  {
    case Grid_Rendertarget4:
      lineStep = 4;
      midX    = renderViewport.w / 2;
      midY    = renderViewport.h / 2;
      limitX  = renderViewport.w / 2;
      limitY  = renderViewport.h / 2;
      break;
    case Grid_Rendertarget16:
      lineStep = 16;
      midX    = renderViewport.w / 2;
      midY    = renderViewport.h / 2;
      limitX  = renderViewport.w / 2;
      limitY  = renderViewport.h / 2;
      break;
    case Grid_Lens:
    {
      lineStep = 48;
      Vector2f rendertargetNDC = FovPort(EyeRenderDesc[eye].Fov).TanAngleToRendertargetNDC(Vector2f(0.0f));
      midX    = (int)( ( rendertargetNDC.x * 0.5f + 0.5f ) * (float)renderViewport.w + 0.5f );
      midY    = (int)( ( rendertargetNDC.y * 0.5f + 0.5f ) * (float)renderViewport.h + 0.5f );
      limitX  = Alg::Max ( renderViewport.w - midX, midX );
      limitY  = Alg::Max ( renderViewport.h - midY, midY );
    }
      break;
    default: OVR_ASSERT ( false ); break;
  }
  
  int spacerMask = (lineStep<<2)-1;
  
  
  for ( int xp = 0; xp < limitX; xp += lineStep )
  {
    float x[4];
    float y[4];
    x[0] = (float)( midX + xp );
    y[0] = (float)0;
    x[1] = (float)( midX + xp );
    y[1] = (float)renderViewport.h;
    x[2] = (float)( midX - xp );
    y[2] = (float)0;
    x[3] = (float)( midX - xp );
    y[3] = (float)renderViewport.h;
    if ( xp == 0 )
    {
      pRender->RenderLines ( 1, cMid, x, y );
    }
    else if ( ( xp & spacerMask ) == 0 )
    {
      pRender->RenderLines ( 2, cSpacer, x, y );
    }
    else
    {
      pRender->RenderLines ( 2, cNormal, x, y );
    }
  }
  for ( int yp = 0; yp < limitY; yp += lineStep )
  {
    float x[4];
    float y[4];
    x[0] = (float)0;
    y[0] = (float)( midY + yp );
    x[1] = (float)renderViewport.w;
    y[1] = (float)( midY + yp );
    x[2] = (float)0;
    y[2] = (float)( midY - yp );
    x[3] = (float)renderViewport.w;
    y[3] = (float)( midY - yp );
    if ( yp == 0 )
    {
      pRender->RenderLines ( 1, cMid, x, y );
    }
    else if ( ( yp & spacerMask ) == 0 )
    {
      pRender->RenderLines ( 2, cSpacer, x, y );
    }
    else
    {
      pRender->RenderLines ( 2, cNormal, x, y );
    }
  }
}




//-------------------------------------------------------------------------------------
// Sets the Platform::Application to VRApp class

OVR_PLATFORM_APP(VRApp);