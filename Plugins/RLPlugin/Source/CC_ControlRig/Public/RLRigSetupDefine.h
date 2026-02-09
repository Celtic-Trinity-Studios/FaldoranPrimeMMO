// Copyright 2024 The Reallusion Authors. All Rights Reserved.

#define PLUGIN_NAME "CC_ControlRig"
#define CONTENT_BROWSERT_CONTEXT_MENU_RIG_SETUP_SECTION_NAME "CC RIGS"
#define CONTENT_BROWSERT_CONTEXT_MENU_RIG_SETUP_ACTION_NAME "Create CC Control Rig"
#define CONTENT_BROWSERT_CONTEXT_MENU_RIG_SETUP_ACTION_TOOLTIP "Add UE Control Rig to CC Characters"
#define NO_SELECTED_ACTORS_RIG_SETUP_MESSAGE "Failed to issue command. Please select a skeletal mesh actor."
#define SELECTED_ACTORS_RIG_SETUP_FINISHED_MESSAGE "The CC Rigs is complete."
#define LIVE_LINK_SKELETON_BEGIN_RIG_SETUP_MESSAGE  "iClone Live Link function will be unavailable after a CC Control Rig is applied.\
                                                     You can re-enable iClone Live Link in the Actor Detail window."
#define LIVE_LINK_SKELETON_NOTCOMPATIBLE_MESSAGE   "This skeleton structure is not supported."

#define RIG_SETUP_FOLDER_SHARE_CONTENT_PATH         "/CC_Rigs"
#define RIG_SETUP_FOLDER_SHARE_PICKER_FOLDER_PATH   "/CC_Rigs/Picker_Assets"
#define RIG_SETUP_FOLDER_SHARE_CONTENT_CONTROL_PATH "/CC_Rigs/Controls"
#define RIG_SETUP_SUFFIX_NAME                       "_Rig_BP"
#define RIG_SETUP_CONTENT_PICKER_FOLDER             "/Picker_Assets"
#define RIG_SETUP_CONTENT_PICKER_FOLDER_PATH        RIG_SETUP_CONTENT_PICKER_FOLDER
#define RIG_SETUP_CONTENT_5_3_PICKER_FOLDER         RIG_SETUP_CONTENT_PICKER_FOLDER + TEXT("/Picker5_3")
#define RIG_SETUP_CONTENT_5_4_PICKER_FOLDER         RIG_SETUP_CONTENT_PICKER_FOLDER + TEXT("/Picker5_4")
#define RIG_SETUP_FACE_ANIMATION_BLUEPRINT_TEMPLATE_NAME                "AB_Face"
#define RIG_SETUP_BODY_ANIMATION_BLUEPRINT_TEMPLATE_NAME                "AB_Body"
#define RIG_SETUP_FACE_WITH_LIVE_LINK_ANIMATION_BLUEPRINT_TEMPLATE_NAME "AB_LL_Face"
#define RIG_SETUP_BODY_WITH_LIVE_LINK_ANIMATION_BLUEPRINT_TEMPLATE_NAME "AB_LL_Body"
#define RIG_SETUP_BLUEPRINT_TEMPLATE_NAME                               "CC_Rig_BP"
#define RIG_SETUP_WITH_LIVE_LINK_BLUEPRINT_TEMPLATE_NAME                "CC_Rig_With_LiveLink_BP"
#define RIG_SETUP_MHA_FACE_RIG_TEMPLATE_NAME                            "CC5_HD_Face_Rig"
#define RIG_SETUP_MHA_FACE_RIG_LIB_NAME                                 "MHGizmoLibrary"
#define RIG_SETUP_FACE_RIG_TEMPLATE_NAME                                "CC_Face_Rig"
#define RIG_SETUP_BODY_RIG_TEMPLATE_NAME                                "CC_Body_Rig"
#define RIG_SETUP_SPINE5_BODY_RIG_TEMPLATE_NAME                         "CC5_UE5_Body_Rig"
#define RIG_SETUP_BP_CODE                                               "RigCode_Character_BP"
#define RIG_SETUP_BP_WITH_LIVE_LINK_CODE                                "RigLiveLinkCode_Character_BP"
#define RIG_SETUP_MHA_MORPH_TARGET_NAME                                 "CTRL_expressions_eyeBlinkL"
#define RigBodyWrinkleAnimBP                                            FString( "RigBodyWrinkleAnimBlueprint" )
#define BODY_RIG_WRINKLE_ANIMATION_BLUEPRINT                            FString( "ABP_CC_Body" )
#define LIVELINK_SUBJECT_NAME "IC SubjectName"
#define SECTION_BODY_INDEX_LIST "BodySectionIndex"
#define SECTION_FACE_INDEX_LIST "FaceSectionIndex"

#define CC_HEAD "head"
#define CC_FACIAL "cc_base_facialbone"
#define CC_JAW_ROOT "cc_base_jawroot"
#define CC_UPPER_JAW "cc_base_upper_jaw"
#define CC_TEETH_01 "cc_base_teeth01"
#define CC_TEETH_02 "cc_base_teeth02"
#define CC_TONGUE_01 "cc_base_tongue01"
#define CC_TONGUE_02 "cc_base_tongue02"
#define CC_TONGUE_03 "cc_base_tongue03"
#define CC_LEFT_EYE "cc_base_l_eye"
#define CC_RIGHT_EYE "cc_base_r_eye"

#define CC_SPINE_01 "spine_01"
#define CC_SPINE_02 "spine_02"
#define CC_SPINE_03 "spine_03"
#define CC_SPINE_04 "spine_04"
#define CC_SPINE_05 "spine_05"