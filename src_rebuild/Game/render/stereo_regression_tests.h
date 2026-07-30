/*
 * REDRIVER2 Stereo Regression Tests
 *
 * Comprehensive regression test suite for stereo rendering system
 * Covers non-stereo baseline, stereo mode compatibility, performance, etc.
 */

#ifndef STEREO_REGRESSION_TESTS_H
#define STEREO_REGRESSION_TESTS_H

#include "stereo_test_harness.h"

// Test registration
void StereoRegressionTests_RegisterAll(void);

// Non-Stereo Regression Tests (IDs 0-9)
TEST_RESULT StereoTest_NonStereo_MissionUndercover(void);
TEST_RESULT StereoTest_NonStereo_TakeARide(void);
TEST_RESULT StereoTest_NonStereo_Arcade(void);
TEST_RESULT StereoTest_NonStereo_Multiplayer(void);
TEST_RESULT StereoTest_NonStereo_Cutscenes(void);
TEST_RESULT StereoTest_NonStereo_MenuUI(void);
TEST_RESULT StereoTest_NonStereo_HUDDisplay(void);
TEST_RESULT StereoTest_NonStereo_Audio(void);
TEST_RESULT StereoTest_NonStereo_Pause(void);
TEST_RESULT StereoTest_NonStereo_SaveLoad(void);

// Stereo Mode Compatibility Tests (IDs 10-49)
TEST_RESULT StereoTest_Mode_AnaglyphSimple(void);
TEST_RESULT StereoTest_Mode_AnaglyphFullColor(void);
TEST_RESULT StereoTest_Mode_SideBySide(void);
TEST_RESULT StereoTest_Mode_TopBottom(void);
TEST_RESULT StereoTest_Mode_Interlaced(void);
TEST_RESULT StereoTest_Mode_Polarized(void);
TEST_RESULT StereoTest_Mode_Checkerboard(void);

// Settings Persistence Tests (IDs 50-59)
TEST_RESULT StereoTest_Persist_ModeValue(void);
TEST_RESULT StereoTest_Persist_SeparationValue(void);
TEST_RESULT StereoTest_Persist_ConvergenceValue(void);
TEST_RESULT StereoTest_Persist_EyeSwapSetting(void);
TEST_RESULT StereoTest_Persist_DebugLogSetting(void);
TEST_RESULT StereoTest_Persist_MultipleSettings(void);
TEST_RESULT StereoTest_Persist_ResetDefaults(void);
TEST_RESULT StereoTest_Persist_ConfigFileValidity(void);
TEST_RESULT StereoTest_Persist_CorruptionHandling(void);

// Input Handling Tests (IDs 60-69)
TEST_RESULT StereoTest_Input_ControllerSteering(void);
TEST_RESULT StereoTest_Input_KeyboardMovement(void);
TEST_RESULT StereoTest_Input_MenuNavigation(void);
TEST_RESULT StereoTest_Input_SettingsAdjustment(void);
TEST_RESULT StereoTest_Input_PauseResume(void);

// Performance Tests (IDs 70-79)
TEST_RESULT StereoTest_Performance_MenuIdle(void);
TEST_RESULT StereoTest_Performance_SimpleMission(void);
TEST_RESULT StereoTest_Performance_DenseTraffic(void);
TEST_RESULT StereoTest_Performance_HighSpeed(void);
TEST_RESULT StereoTest_Performance_MemoryLeakDetection(void);

// Edge Case Tests (IDs 80-99)
TEST_RESULT StereoTest_Edge_ExtremeMinSeparation(void);
TEST_RESULT StereoTest_Edge_ExtremeMaxSeparation(void);
TEST_RESULT StereoTest_Edge_ExtremeMinConvergence(void);
TEST_RESULT StereoTest_Edge_ExtremeMaxConvergence(void);
TEST_RESULT StereoTest_Edge_ResolutionChange(void);
TEST_RESULT StereoTest_Edge_FullscreenToggle(void);
TEST_RESULT StereoTest_Edge_AltTabHandling(void);
TEST_RESULT StereoTest_Edge_ModeSwitch(void);
TEST_RESULT StereoTest_Edge_RapidModeSwitching(void);
TEST_RESULT StereoTest_Edge_ConfigCorruption(void);

// Audio Tests (IDs 100-109)
TEST_RESULT StereoTest_Audio_EngineSound(void);
TEST_RESULT StereoTest_Audio_SirenSound(void);
TEST_RESULT StereoTest_Audio_Music(void);
TEST_RESULT StereoTest_Audio_Sync(void);
TEST_RESULT StereoTest_Audio_NoRegression(void);

// Utility test functions
void StereoRegressionTests_RunAllTests(void);
void StereoRegressionTests_RunNonStereoTests(void);
void StereoRegressionTests_RunStereoModeTests(void);
void StereoRegressionTests_RunPersistenceTests(void);
void StereoRegressionTests_RunPerformanceTests(void);
void StereoRegressionTests_GenerateReport(const char *output_dir);

#endif // STEREO_REGRESSION_TESTS_H
