#pragma once

#define Version_Error_Message       "The current procedure has come to a stop because auto-setup is not support for this version of CC / iClone. Please update to the appropriate version."
#define Version_Not_Suitable        "Auto-setup is not support for this version of CC / iClone and some issues may occur as a result. Please update to the appropriate version."

// folder name
#define FOLDER_TEXTURE "textures"

// for IK Rig JSON
#define JSON_IK_RIG_BONE_CHAIN_OBJECT "Bone Chain"
#define JSON_IK_RIG_START_BONE "Start Bone Name"
#define JSON_IK_RIG_END_BONE "End Bone Name"
#define JSON_IK_RIG_IK_GOAL "IK Goal"
#define JSON_IK_RIG_IK_PARENT_OBJECT "IK Goal Parent"
#define JSON_IK_RIG_IK_PARENT "Parent"
#define JSON_IK_RIG_IK_FK_CHAIN_OBJECT "FK Chain"
#define JSON_IK_RIG_IK_FK_CHAIN_ROTATION "Rotation Mode"
#define JSON_IK_RIG_PIN_BONE_OBJECT "Pin Bone"
#define JSON_IK_RIG_PIN_BONE_COPY_FROM "Bone To Copy From"
#define JSON_IK_RIG_PIN_BONE_COPY_TO "Bone To Copy To"
#define JSON_IK_RIG_ALIGN_POLE_VECTOR "Align Pole Vector"


// for physic constraint
#define JSON_CONSTRAINT                             "Physic Contraint"
#define JSON_CONSTRAINT_PARENT                      "Parent Bone"
#define JSON_CONSTRAINT_SWING_1_LIMIT               "Swing 1 Limit"
#define JSON_CONSTRAINT_SWING_2_LIMIT               "Swing 2 Limit"
#define JSON_CONSTRAINT_TWIST_LIMIT                 "Twist Limit"
#define JSON_CONSTRAINT_SWING_LIMIT_PARAMETER       "Swing Limit Parameter"
#define JSON_CONSTRAINT_TWIST_LIMIT_PARAMETER       "Twist Limit Parameter"
#define JSON_CONSTRAINT_PARAMETER_SOFT_CONSTRAINT   "SoftConstraint"
#define JSON_CONSTRAINT_PARAMETER_STIFFNESS         "Stiffness"
#define JSON_CONSTRAINT_PARAMETER_DAMPING           "Damping"
#define JSON_CONSTRAINT_PARAMETER_RESTITUTION       "Restitution"
#define JSON_CONSTRAINT_TARGET_VELOCITY             "Target Velocity"
#define JSON_CONSTRAINT_TARGET_VELOCITY_DRIVE       "Drives"
#define JSON_CONSTRAINT_TARGET_VELOCITY_DRIVE_SLERP "SLERP"
#define JSON_CONSTRAINT_TARGET_VELOCITY_DRIVE_TWIST "Twist"
#define JSON_CONSTRAINT_TARGET_VELOCITY_DRIVE_SWING "Swing"
#define JSON_CONSTRAINT_TARGET_VELOCITY_DAMPING     "Damping"

// for IK Rig
#define IK_RIG_UNREAL4_FILE "Unreal4_IK_Rig"
#if ( ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 6 )
#define IK_RIG_UNREAL5_FILE "Unreal56_IK_Rig"
#else
#define IK_RIG_UNREAL5_FILE "Unreal5_IK_Rig"
#endif

//Digital Hunam Shader Name
#define DigitalTongue L"RLTongue"
#define DigitalTeeth L"RLTeethGum"
#define DigitalEye L"RLEye"
#define DigitalHead L"RLHead"
#define DigitalSkin L"RLSkin"
#define DigitalTearline L"RLEyeTearline"
#define DigitalTearlinePlus L"RLEyeTearline_Plus"
#define DigitalOcclusion L"RLEyeOcclusion"
#define DigitalOcclusionPlus L"RLEyeOcclusion_Plus"
#define RLHair L"RLHair"
#define RLGeneralSSS L"RLSSS"

//Material Keyword
#define UpperTeethKeyWord           "std_upper_teeth"
#define LowerTeethKeyWord           "std_lower_teeth"
#define TougueKeyWord               "tongue"
#define TransparencyKeyWord         "_transparency"
#define ScalpKeyWord                "scalp"
#define EyelashKeyWord              "eyelash"
#define CorneaKeyWord               "std_cornea_"

#define HeadKeyWord                 "head"
#define TearKeyWord                 "tear"
#define EyeOcclusionKeyWord         "eye_occlusion"

//Character UID
#define CC_G1_UID L"RL_G6_Standard_Series"
#define CC_G3_UID L"RL_CharacterCreator_Base_Std_G3"
#define CC_G3PLUS_UID L"RL_CC3_Plus"
#define CC_GAME_BASE_MULTI_UID L"RL_CharacterCreator_Base_Game_G1_Multi_UV"
#define CC_GAME_BASE_ONE_UID L"RL_CharacterCreator_Base_Game_G1_One_UV"
#define CC_GAME_BASE_DIVIDE_EYELASH_UID L"RL_CharacterCreator_Base_Game_G1_Divide_Eyelash_UV"
#define TYPE_CD_IC7_UID L"NonStdLookAtDataCopyFromCCBase"
#define TYPE_C_UID    L"ActorBuild"
#define TYPE_D_UID    L"ActorScan"
#define TYPE_ACCU_RIG L"AccuRig"

#define TYPE_LOD L"lod"

#define BONE_TYPE_G1          "G1"
#define BONE_TYPE_G3          "G3"
#define BONE_TYPE_G3PLUS      "G3Plus"
#define BONE_TYPE_GAME_MULTI  "GameBase_Multi"
#define BONE_TYPE_GAME_SINGLE "GameBase_Single"

#if ENGINE_MAJOR_VERSION < 5
#define MeshGetMaterial StaticMaterials
#else
#define MeshGetMaterial GetStaticMaterials()
#endif
static TMap<FString, FString> CharacterGenerationBoneMap =
{
    { CC_G1_UID,                        BONE_TYPE_G1 },
    { CC_G3_UID,                        BONE_TYPE_G3 },
    { CC_G3PLUS_UID,                    BONE_TYPE_G3PLUS },
    { TYPE_CD_IC7_UID,                  BONE_TYPE_G3PLUS },
    { TYPE_C_UID,                       BONE_TYPE_G3PLUS },
    { TYPE_D_UID,                       BONE_TYPE_G3PLUS },
    { TYPE_ACCU_RIG,                    BONE_TYPE_G3PLUS },
    { CC_GAME_BASE_MULTI_UID,           BONE_TYPE_GAME_MULTI },
    { CC_GAME_BASE_ONE_UID,             BONE_TYPE_GAME_SINGLE },
    { CC_GAME_BASE_DIVIDE_EYELASH_UID,  BONE_TYPE_GAME_SINGLE }
};

#define HeadKeyWordG1                 "skin_head"

static TMap<FString, FString> BoneTypeSkinKeyWord =
{
    { BONE_TYPE_G1,          "skin_body" }, // skin_head
    { BONE_TYPE_G3,          "std_skin_" },
    { BONE_TYPE_G3PLUS,      "std_skin_" },
    { BONE_TYPE_GAME_MULTI,  "ga_skin_" },
    { BONE_TYPE_GAME_SINGLE, "ga_skin_" }
};

#define UE_BONE_NAME_PELVIS "pelvis"

#define MATERIAL_FOLDER_NAME "Materials"
#define SUBSURFACE_PROFILE_FOLDER_NAME  "SSSProfile"

#define CCSHADER_FOLDER                     FString( "/Game/CC_Shaders" )
#define TEAR_PACKAGE_NAME                   FString( CCSHADER_FOLDER + "/TearLineShader/RL_TearLine" )
#define TEAR_PLUS_PACKAGE_NAME              FString( CCSHADER_FOLDER + "/TearLineShader/RL_TearLine_Plus" )
#define EYE_OCCLUSION_PACKAGE_NAME          FString( CCSHADER_FOLDER + "/EyeOcclusionShader/RL_EyeOcclusion" )
#define EYE_OCCLUSION_PLUS_PACKAGE_NAME     FString( CCSHADER_FOLDER + "/EyeOcclusionShader/RL_EyeOcclusion_Plus" )
#define HAIR_PACKAGE_NAME                   FString( CCSHADER_FOLDER + "/HairShader/RL_Hair" )
#define HAIR_SPECULAR_PACKAGE_NAME          FString( CCSHADER_FOLDER + "/HairShader/RL_Hair_Specular" )
#define EYELASH_PACKAGE_NAME                FString( CCSHADER_FOLDER + "/EyelashShader/RL_Eyelash" )
#define EYE_PACKAGE_NAME                    FString( CCSHADER_FOLDER + "/EyeShader/RL_EyeRefractive" )
#define HQ_SKIN_PACKAGE_NAME                FString( CCSHADER_FOLDER + "/SkinShader/RL_HQSkin" )
#define LW_SKIN_PACKAGE_NAME                FString( CCSHADER_FOLDER + "/SkinShader/RL_LWSkin" )
#define GUMS_TONGUE_TEETH_PACKAGE_NAME      FString( CCSHADER_FOLDER + "/GumsTongueTeethShader/RL_GumsTongueTeeth" )
#define STANDARD_ORM_PACKAGE_NAME           FString( CCSHADER_FOLDER + "/StandardShader/RL_Standard" )
#define STANDARD_ORM_OPACITY_PACKAGE_NAME   FString( CCSHADER_FOLDER + "/StandardShader/RL_Standard_Opacity" )
#define GENERAL_SSS_PACKAGE_NAME            FString( CCSHADER_FOLDER + "/RL_SSS/RL_SSS" )
static FString strPluginFolderPath = "";

// Bone Names
static TArray<FString> CCIkBoneRootNames =
{
    "ik_foot_root",
    "ik_hand_root"
};

static TArray<FString> CCBoneConvertedNamesUE4 =
{
    "clavicle_l",
    "upperarm_l",
    "lowerarm_l",
    "lowerarm_twist_01_l",
    "hand_l",
    "thumb_01_l",
    "thumb_02_l",
    "thumb_03_l",
    "index_01_l",
    "index_02_l",
    "index_03_l",
    "middle_01_l",
    "middle_02_l",
    "middle_03_l",
    "ring_01_l",
    "ring_02_l",
    "ring_03_l",
    "pinky_01_l",
    "pinky_02_l",
    "pinky_03_l",
    "thigh_l",
    "calf_l",
    "foot_l",
    "ball_l",
    "clavicle_r",
    "upperarm_r",
    "lowerarm_r",
    "lowerarm_twist_01_r",
    "hand_r",
    "thumb_01_r",
    "thumb_02_r",
    "thumb_03_r",
    "index_01_r",
    "index_02_r",
    "index_03_r",
    "middle_01_r",
    "middle_02_r",
    "middle_03_r",
    "ring_01_r",
    "ring_02_r",
    "ring_03_r",
    "pinky_01_r",
    "pinky_02_r",
    "pinky_03_r",
    "thigh_r",
    "calf_r",
    "foot_r",
    "ball_r",
    "head",
    "neck_01",
    "spine_03",
    "spine_02",
    "spine_01",
    "pelvis",
    "root",
    "upperarm_twist_01_r",
    "thigh_twist_01_r",
    "calf_twist_01_r",
    "upperarm_twist_01_l",
    "thigh_twist_01_l",
    "calf_twist_01_l"
};

static TArray<FString> CCBoneConvertedNamesUE5 =
{
    "clavicle_l",
    "upperarm_l",
    "lowerarm_l",
    "lowerarm_twist_02_l",
    "hand_l",
    "thumb_01_l",
    "thumb_02_l",
    "thumb_03_l",
    "index_metacarpal_l",
    "index_01_l",
    "index_02_l",
    "index_03_l",
    "middle_metacarpal_l",
    "middle_01_l",
    "middle_02_l",
    "middle_03_l",
    "ring_metacarpal_l",
    "ring_01_l",
    "ring_02_l",
    "ring_03_l",
    "pinky_metacarpal_l",
    "pinky_01_l",
    "pinky_02_l",
    "pinky_03_l",
    "thigh_l",
    "calf_l",
    "foot_l",
    "ball_l",
    "clavicle_r",
    "upperarm_r",
    "lowerarm_r",
    "lowerarm_twist_02_r",
    "hand_r",
    "thumb_01_r",
    "thumb_02_r",
    "thumb_03_r",
    "index_metacarpal_r",
    "index_01_r",
    "index_02_r",
    "index_03_r",
    "middle_metacarpal_r",
    "middle_01_r",
    "middle_02_r",
    "middle_03_r",
    "ring_metacarpal_r",
    "ring_01_r",
    "ring_02_r",
    "ring_03_r",
    "pinky_metacarpal_r",
    "pinky_01_r",
    "pinky_02_r",
    "pinky_03_r",
    "thigh_r",
    "calf_r",
    "foot_r",
    "ball_r",
    "head",
    "neck_01",
    "neck_02",
    "spine_05",
    "spine_04",
    "spine_03",
    "spine_02",
    "spine_01",
    "pelvis",
    "root",
    "upperarm_twist_01_r",
    "upperarm_twist_02_r",
    "thigh_twist_01_r",
    "calf_twist_02_r",
    "upperarm_twist_01_l",
    "upperarm_twist_02_l",
    "thigh_twist_01_l",
    "calf_twist_02_l",
};

// CurveNames
static TArray<FString> m_kAddHDExpCurvesList =
{
    "HeadControlSwitch",
    "HeadTranslationX",
    "HeadTranslationY",
    "HeadTranslationZ",
    "HeadYaw",
    "HeadPitch",
    "HeadRoll",
    "CTRL_rigLogic_OffOn",
    "LeftEyeYaw",
    "LeftEyePitch",
    "LeftEyeRoll",
    "RightEyeYaw",
    "RightEyePitch",
    "RightEyeRoll"
};

static TArray<FString> m_kAddExtendExpCurvesList =
{
    "CTRL_expressions_tongueWide",
    "CTRL_expressions_tongueUp",
    "CTRL_expressions_tongueTwistRight",
    "CTRL_expressions_tongueTwistLeft",
    "CTRL_expressions_tongueTipUp",
    "CTRL_expressions_tongueTipRight",
    "CTRL_expressions_tongueTipLeft",
    "CTRL_expressions_tongueTipDown",
    "CTRL_expressions_tongueThin",
    "CTRL_expressions_tongueThick",
    "CTRL_expressions_tongueRollUp",
    "CTRL_expressions_tongueRollRight",
    "CTRL_expressions_tongueRollLeft",
    "CTRL_expressions_tongueRollDown",
    "CTRL_expressions_tongueRoll",
    "CTRL_expressions_tongueRight",
    "CTRL_expressions_tonguePress",
    "CTRL_expressions_tongueOut",
    "CTRL_expressions_tongueNarrow",
    "CTRL_expressions_tongueLeft",
    "CTRL_expressions_tongueIn",
    "CTRL_expressions_tongueDown",
    "CTRL_expressions_tongueBendUp",
    "CTRL_expressions_tongueBendDown",
    "CTRL_expressions_teethUpU",
    "CTRL_expressions_teethUpD",
    "CTRL_expressions_teethRightU",
    "CTRL_expressions_teethRightD",
    "CTRL_expressions_teethLeftU",
    "CTRL_expressions_teethLeftD",
    "CTRL_expressions_teethFwdU",
    "CTRL_expressions_teethFwdD",
    "CTRL_expressions_teethDownU",
    "CTRL_expressions_teethDownD",
    "CTRL_expressions_teethBackU",
    "CTRL_expressions_teethBackD",
    "CTRL_expressions_skullUnified",
    "CTRL_expressions_noseWrinkleUpperR",
    "CTRL_expressions_noseWrinkleUpperL",
    "CTRL_expressions_noseWrinkleR",
    "CTRL_expressions_noseWrinkleL",
    "CTRL_expressions_noseNostrilDilateR",
    "CTRL_expressions_noseNostrilDilateL",
    "CTRL_expressions_noseNostrilDepressR",
    "CTRL_expressions_noseNostrilDepressL",
    "CTRL_expressions_noseNostrilCompressR",
    "CTRL_expressions_noseNostrilCompressL",
    "CTRL_expressions_noseNasolabialDeepenR",
    "CTRL_expressions_noseNasolabialDeepenL",
    "CTRL_expressions_neckThroatUp",
    "CTRL_expressions_neckThroatInhale",
    "CTRL_expressions_neckThroatExhale",
    "CTRL_expressions_neckThroatDown",
    "CTRL_expressions_neckSwallowPh5",
    "CTRL_expressions_neckSwallowPh4",
    "CTRL_expressions_neckSwallowPh3",
    "CTRL_expressions_neckSwallowPh2",
    "CTRL_expressions_neckSwallowPh1",
    "CTRL_expressions_neckStretchR",
    "CTRL_expressions_neckStretchL",
    "CTRL_expressions_neckMastoidContractR",
    "CTRL_expressions_neckMastoidContractL",
    "CTRL_expressions_neckDigastricUp",
    "CTRL_expressions_neckDigastricDown",
    "CTRL_expressions_mouthUpperLipTowardsTeethRv",
    "CTRL_expressions_mouthUpperLipTowardsTeethLv",
    "CTRL_expressions_mouthUpperLipShiftRight",
    "CTRL_expressions_mouthUpperLipShiftLeft",
    "CTRL_expressions_mouthUpperLipRollOutR",
    "CTRL_expressions_mouthUpperLipRollOutL",
    "CTRL_expressions_mouthUpperLipRollInR",
    "CTRL_expressions_mouthUpperLipRollInL",
    "CTRL_expressions_mouthUpperLipRaiseR",
    "CTRL_expressions_mouthUpperLipRaiseL",
    "CTRL_expressions_mouthUpperLipBiteR",
    "CTRL_expressions_mouthUpperLipBiteL",
    "CTRL_expressions_mouthUp",
    "CTRL_expressions_mouthStretchR",
    "CTRL_expressions_mouthStretchLipsCloseR",
    "CTRL_expressions_mouthStretchLipsCloseL",
    "CTRL_expressions_mouthStretchL",
    "CTRL_expressions_mouthStickyUOUTR",
    "CTRL_expressions_mouthStickyUOUTL",
    "CTRL_expressions_mouthStickyUINR",
    "CTRL_expressions_mouthStickyUINL",
    "CTRL_expressions_mouthStickyUC",
    "CTRL_expressions_mouthStickyDOUTR",
    "CTRL_expressions_mouthStickyDOUTL",
    "CTRL_expressions_mouthStickyDINR",
    "CTRL_expressions_mouthStickyDINL",
    "CTRL_expressions_mouthStickyDC",
    "CTRL_expressions_mouthSharpCornerPullR",
    "CTRL_expressions_mouthSharpCornerPullL",
    "CTRL_expressions_mouthRight",
    "CTRL_expressions_mouthPressUR",
    "CTRL_expressions_mouthPressUL",
    "CTRL_expressions_mouthPressDR",
    "CTRL_expressions_mouthPressDL",
    "CTRL_expressions_mouthLowerLipTowardsTeethR",
    "CTRL_expressions_mouthLowerLipTowardsTeethL",
    "CTRL_expressions_mouthLowerLipShiftRight",
    "CTRL_expressions_mouthLowerLipShiftLeft",
    "CTRL_expressions_mouthLowerLipRollOutR",
    "CTRL_expressions_mouthLowerLipRollOutL",
    "CTRL_expressions_mouthLowerLipRollInR",
    "CTRL_expressions_mouthLowerLipRollInL",
    "CTRL_expressions_mouthLowerLipDepressR",
    "CTRL_expressions_mouthLowerLipDepressL",
    "CTRL_expressions_mouthLowerLipBiteR",
    "CTRL_expressions_mouthLowerLipBiteL",
    "CTRL_expressions_mouthLipsTowardsUR",
    "CTRL_expressions_mouthLipsTowardsUL",
    "CTRL_expressions_mouthLipsTowardsDR",
    "CTRL_expressions_mouthLipsTowardsDL",
    "CTRL_expressions_mouthLipsTogetherUR",
    "CTRL_expressions_mouthLipsTogetherUL",
    "CTRL_expressions_mouthLipsTogetherDR",
    "CTRL_expressions_mouthLipsTogetherDL",
    "CTRL_expressions_mouthLipsTightenUR",
    "CTRL_expressions_mouthLipsTightenUL",
    "CTRL_expressions_mouthLipsTightenDR",
    "CTRL_expressions_mouthLipsTightenDL",
    "CTRL_expressions_mouthLipsThinUR",
    "CTRL_expressions_mouthLipsThinUL",
    "CTRL_expressions_mouthLipsThinInwardUR",
    "CTRL_expressions_mouthLipsThinInwardUL",
    "CTRL_expressions_mouthLipsThinInwardDR",
    "CTRL_expressions_mouthLipsThinInwardDL",
    "CTRL_expressions_mouthLipsThinDR",
    "CTRL_expressions_mouthLipsThinDL",
    "CTRL_expressions_mouthLipsThickUR",
    "CTRL_expressions_mouthLipsThickUL",
    "CTRL_expressions_mouthLipsThickInwardUR",
    "CTRL_expressions_mouthLipsThickInwardUL",
    "CTRL_expressions_mouthLipsThickInwardDR",
    "CTRL_expressions_mouthLipsThickInwardDL",
    "CTRL_expressions_mouthLipsThickDR",
    "CTRL_expressions_mouthLipsThickDL",
    "CTRL_expressions_mouthLipsStickyRPh3",
    "CTRL_expressions_mouthLipsStickyRPh2",
    "CTRL_expressions_mouthLipsStickyRPh1",
    "CTRL_expressions_mouthLipsStickyLPh3",
    "CTRL_expressions_mouthLipsStickyLPh2",
    "CTRL_expressions_mouthLipsStickyLPh1",
    "CTRL_expressions_mouthLipsPushUR",
    "CTRL_expressions_mouthLipsPushUL",
    "CTRL_expressions_mouthLipsPushDR",
    "CTRL_expressions_mouthLipsPushDL",
    "CTRL_expressions_mouthLipsPurseUR",
    "CTRL_expressions_mouthLipsPurseUL",
    "CTRL_expressions_mouthLipsPurseDR",
    "CTRL_expressions_mouthLipsPurseDL",
    "CTRL_expressions_mouthLipsPullUR",
    "CTRL_expressions_mouthLipsPullUL",
    "CTRL_expressions_mouthLipsPullDR",
    "CTRL_expressions_mouthLipsPullDL",
    "CTRL_expressions_mouthLipsPressR",
    "CTRL_expressions_mouthLipsPressL",
    "CTRL_expressions_mouthLipsBlowR",
    "CTRL_expressions_mouthLipsBlowL",
    "CTRL_expressions_mouthLeft",
    "CTRL_expressions_mouthFunnelUR",
    "CTRL_expressions_mouthFunnelUL",
    "CTRL_expressions_mouthFunnelDR",
    "CTRL_expressions_mouthFunnelDL",
    "CTRL_expressions_mouthDown",
    "CTRL_expressions_mouthDimpleR",
    "CTRL_expressions_mouthDimpleL",
    "CTRL_expressions_mouthCornerWideR",
    "CTRL_expressions_mouthCornerWideL",
    "CTRL_expressions_mouthCornerUpR",
    "CTRL_expressions_mouthCornerUpL",
    "CTRL_expressions_mouthCornerSharpenUR",
    "CTRL_expressions_mouthCornerSharpenUL",
    "CTRL_expressions_mouthCornerSharpenDR",
    "CTRL_expressions_mouthCornerSharpenDL",
    "CTRL_expressions_mouthCornerRounderUR",
    "CTRL_expressions_mouthCornerRounderUL",
    "CTRL_expressions_mouthCornerRounderDR",
    "CTRL_expressions_mouthCornerRounderDL",
    "CTRL_expressions_mouthCornerPullR",
    "CTRL_expressions_mouthCornerPullL",
    "CTRL_expressions_mouthCornerNarrowR",
    "CTRL_expressions_mouthCornerNarrowL",
    "CTRL_expressions_mouthCornerDownR",
    "CTRL_expressions_mouthCornerDownL",
    "CTRL_expressions_mouthCornerDepressR",
    "CTRL_expressions_mouthCornerDepressL",
    "CTRL_expressions_mouthCheekSuckR",
    "CTRL_expressions_mouthCheekSuckL",
    "CTRL_expressions_mouthCheekBlowR",
    "CTRL_expressions_mouthCheekBlowL",
    "CTRL_expressions_lookAtSwitch",
    "CTRL_expressions_jawRight",
    "CTRL_expressions_jawOpenExtreme",
    "CTRL_expressions_jawOpen",
    "CTRL_expressions_jawLeft",
    "CTRL_expressions_jawFwd",
    "CTRL_expressions_jawClenchR",
    "CTRL_expressions_jawClenchL",
    "CTRL_expressions_jawChinRaiseUR",
    "CTRL_expressions_jawChinRaiseUL",
    "CTRL_expressions_jawChinRaiseDR",
    "CTRL_expressions_jawChinRaiseDL",
    "CTRL_expressions_jawChinCompressR",
    "CTRL_expressions_jawChinCompressL",
    "CTRL_expressions_jawBack",
    "CTRL_expressions_headTurnUpU",
    "CTRL_expressions_headTurnUpM",
    "CTRL_expressions_headTurnUpD",
    "CTRL_expressions_headTurnRightU",
    "CTRL_expressions_headTurnRightM",
    "CTRL_expressions_headTurnRightD",
    "CTRL_expressions_headTurnLeftU",
    "CTRL_expressions_headTurnLeftM",
    "CTRL_expressions_headTurnLeftD",
    "CTRL_expressions_headTurnDownU",
    "CTRL_expressions_headTurnDownM",
    "CTRL_expressions_headTurnDownD",
    "CTRL_expressions_headTiltRightU",
    "CTRL_expressions_headTiltRightM",
    "CTRL_expressions_headTiltRightD",
    "CTRL_expressions_headTiltLeftU",
    "CTRL_expressions_headTiltLeftM",
    "CTRL_expressions_headTiltLeftD",
    "CTRL_expressions_eyeWidenR",
    "CTRL_expressions_eyeWidenL",
    "CTRL_expressions_eyeUpperLidUpR",
    "CTRL_expressions_eyeUpperLidUpL",
    "CTRL_expressions_eyeSquintInnerR",
    "CTRL_expressions_eyeSquintInnerL",
    "CTRL_expressions_eyeRelaxR",
    "CTRL_expressions_eyeRelaxL",
    "CTRL_expressions_eyePupilWideR",
    "CTRL_expressions_eyePupilWideL",
    "CTRL_expressions_eyePupilNarrowR",
    "CTRL_expressions_eyePupilNarrowL",
    "CTRL_expressions_eyeParallelLookDirection",
    "CTRL_expressions_eyeLowerLidUpR",
    "CTRL_expressions_eyeLowerLidUpL",
    "CTRL_expressions_eyeLowerLidDownR",
    "CTRL_expressions_eyeLowerLidDownL",
    "CTRL_expressions_eyeLookUpR",
    "CTRL_expressions_eyeLookUpL",
    "CTRL_expressions_eyeLookRightR",
    "CTRL_expressions_eyeLookRightL",
    "CTRL_expressions_eyeLookLeftR",
    "CTRL_expressions_eyeLookLeftL",
    "CTRL_expressions_eyeLookDownR",
    "CTRL_expressions_eyeLookDownL",
    "CTRL_expressions_eyeLidPressR",
    "CTRL_expressions_eyeLidPressL",
    "CTRL_expressions_eyelashesUpOUTR",
    "CTRL_expressions_eyelashesUpOUTL",
    "CTRL_expressions_eyelashesUpINR",
    "CTRL_expressions_eyelashesUpINL",
    "CTRL_expressions_eyelashesDownOUTR",
    "CTRL_expressions_eyelashesDownOUTL",
    "CTRL_expressions_eyelashesDownINR",
    "CTRL_expressions_eyelashesDownINL",
    "CTRL_expressions_eyeFaceScrunchR",
    "CTRL_expressions_eyeFaceScrunchL",
    "CTRL_expressions_eyeCheekRaiseR",
    "CTRL_expressions_eyeCheekRaiseL",
    "CTRL_expressions_eyeBlinkR",
    "CTRL_expressions_eyeBlinkL",
    "CTRL_expressions_earUpR",
    "CTRL_expressions_earUpL",
    "CTRL_expressions_browRaiseOuterR",
    "CTRL_expressions_browRaiseOuterL",
    "CTRL_expressions_browRaiseInR",
    "CTRL_expressions_browRaiseInL",
    "CTRL_expressions_browLateralR",
    "CTRL_expressions_browLateralL",
    "CTRL_expressions_browDownR",
    "CTRL_expressions_browDownL",
    "CTRL_rigLogic_OffOn",
    "HeadControlSwitch",
    "HeadTranslationX",
    "HeadTranslationY",
    "HeadTranslationZ",
    "HeadYaw",
    "HeadPitch",
    "HeadRoll",
    "LeftEyeYaw",
    "LeftEyePitch",
    "LeftEyeRoll",
    "RightEyeYaw",
    "RightEyePitch",
    "RightEyeRoll"
};