#ifndef STEREO_QUALITY_TESTS_H
#define STEREO_QUALITY_TESTS_H

// Run comprehensive test suite for stereo quality module
int StereoQuality_RunTests(void);

// Individual test suites (can be run separately)
void Test_Initialization(void);
void Test_AnaglyphMatrices(void);
void Test_ChromaticAberration(void);
void Test_EyeSeparation(void);
void Test_TemporalFiltering(void);
void Test_EdgeBlending(void);
void Test_SceneAnalysis(void);
void Test_ToneMapping(void);
void Test_ShaderGeneration(void);
void Test_SettingsManagement(void);

#endif // STEREO_QUALITY_TESTS_H
