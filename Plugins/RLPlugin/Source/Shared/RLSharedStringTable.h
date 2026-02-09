#pragma once

#define UNREAL_ASSET_FORMAT        FString( ".uasset" )
#define MAIN_PLUGIN_NAME           "RLPlugin"
// for metadata
#define METADATA_BUILD_CONTROL_RIG "BuildControlRig"
#define METADATA_EXPRESSION_SET "ExpressionSet"

// for expression set
#define EXPRESSION_SET_EXTEND "CC4_Extend"
#define EXPRESSION_SET_METAHUMAN "CC5_HD"

// for Content path
#define CONTENT_COMMON    FString( "/Content/RL_Common" )
#define CONTENT_ANIMATION CONTENT_COMMON + "/Animation"
#define CONTENT_CONTROL   CONTENT_COMMON + "/Control"
#define CONTENT_MATERIAL  CONTENT_COMMON + "/Material"
#define CONTENT_UTILITY   CONTENT_COMMON + "/Utility"
#define CONTENT_LIVELINK  CONTENT_COMMON + "/LiveLink"

// for File name
#define WrinkleAnimBP "ABP_RL_Rig_Logic"

// for Setup Wrinkle BP
#define ExpSequence       "AS_ExpSequence" 
#define ExpPoseAsset      "PA_ExtendFacialProfile" 
#define ExpPoseAsset_MHA  "PA_HDFacialProfile" 

// for content browser folder
#define FOLDER_POST_PROCESS "Post_Process"
#define FOLDER_RIG "Rigs"
#define FOLDER_MOTION "Motion"
#define FOLDER_GROOM "Groom"

// for asset name
#define BLUEPRINT_SUFFIX                    FString( "_Blueprint" )
#define ANIMATION_BLUEPRINT_SUFFIX          FString( "_AnimationBlueprint" )
#define LIVELINK_BLUEPRINT_SUFFIX           BLUEPRINT_SUFFIX + "_LiveLink"
#define LIVELINK_ANIMATION_BLUEPRINT_SUFFIX ANIMATION_BLUEPRINT_SUFFIX + "_LiveLink"
#define CONTROL_RIG_BLUEPRINT_SUFFIX           BLUEPRINT_SUFFIX + "_CC_ControlRig"
#define CONTROL_RIG_ANIMATION_BLUEPRINT_SUFFIX ANIMATION_BLUEPRINT_SUFFIX + "_CC_ControlRig"