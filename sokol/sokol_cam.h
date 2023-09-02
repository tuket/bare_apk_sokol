#if defined(SOKOL_IMPL) && !defined(SOKOL_CAM_IMPL)
	#define SOKOL_CAM_IMPL
#endif

#ifndef SOKOL_CAM_INCLUDED
/*
	Documentation here
*/

#define SOKOL_CAM_USE_NDK_TO_GET_CAM_LIST 0
	// you can either use the NDK API or the SDK API.
	// the NDK API is easier to use, the SDK API is much more verbose
	// however, the SDK API doesn't seem to work! ACameraManager_getCameraIdList return zero cameras!
	// another disadvantage of the NDK API for cameras is that it requires API 24 (android 7)

#define SOKOL_CAM_INCLUDED (1)
#include <stdint.h>

#if defined(SOKOL_API_DECL) && !defined(SOKOL_CAM_API_DECL)
	#define SOKOL_CAM_API_DECL SOKOL_API_DECL
#endif
#ifndef SOKOL_TIME_API_DECL
	#if defined(_WIN32) && defined(SOKOL_DLL) && defined(SOKOL_CAM_IMPL)
		#define SOKOL_CAM_API_DECL __declspec(dllexport)
	#elif defined(_WIN32) && defined(SOKOL_DLL)
		#define SOKOL_CAM_API_DECL __declspec(dllimport)
	#else
		#define SOKOL_CAM_API_DECL extern
	#endif
#endif

#if !defined(SCAM_MAX_CAMERAS)
	#define SCAM_MAX_CAMERAS (12) // assuming a maximum of cameras by default
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum scam_camera_type : uint8_t {
	SCAM_CAMERA_TYPE_MAIN, // camera facing opposite to the screen
	SCAM_CAMERA_TYPE_SELFIE,
	SCAM_CAMERA_TYPE_EXTERNAL, // externally connected camera e.g USB
} scam_camera_type;

typedef struct scam_camera_list_t {
	uint32_t num_cameras;
	scam_camera_type types[SCAM_MAX_CAMERAS];
	const char** ids;
#if !SOKOL_CAM_USE_NDK_TO_GET_CAM_LIST
	void *_internal_handle;
#endif
	uint32_t err_code;
} scam_camera_list_t;

SOKOL_CAM_API_DECL void scam_setup(void);
SOKOL_CAM_API_DECL void scam_shutdown(void);
SOKOL_CAM_API_DECL scam_camera_list_t scam_get_camera_list();
SOKOL_CAM_API_DECL void scam_delete_camera_list(scam_camera_list_t* l);
SOKOL_CAM_API_DECL void scam_turn_on_camera(uint32_t cam_id);
SOKOL_CAM_API_DECL void scam_turn_off_camera(uint32_t cam_id);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif // SOKOL_TIME_INCLUDED

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#if defined(SOKOL_CAM_IMPL)

#ifndef SOKOL_API_IMPL
	#define SOKOL_API_IMPL
#endif
#ifndef SOKOL_DEBUG
	#ifndef NDEBUG
		#define SOKOL_DEBUG
	#endif
#endif
#ifndef SOKOL_ASSERT
	#include <assert.h>
	#define SOKOL_ASSERT(c) assert(c)
#endif

#ifndef _SOKOL_UNUSED
	#define _SOKOL_UNUSED(x) (void)(x)
#endif

#ifndef _SOKOL_PRIVATE
	#if defined(__GNUC__) || defined(__clang__)
		#define _SOKOL_PRIVATE __attribute__((unused)) static
	#else
		#define _SOKOL_PRIVATE static
	#endif
#endif

#if defined(__ANDROID__)
	#define _SAPP_ANDROID (1)
#else
	#error("sokol_cam not supported in this platform")
#endif

#if defined(_SAPP_ANDROID)
/*-- ANDROID -----------------------------------------------------------------*/

#include <camera/NdkCameraManager.h>

#if !SOKOL_CAM_USE_NDK_TO_GET_CAM_LIST
	static ACameraManager *s_camera_manager;
#endif

SOKOL_API_IMPL void scam_setup()
{
#if !SOKOL_CAM_USE_NDK_TO_GET_CAM_LIST
	s_camera_manager = ACameraManager_create();
#endif
}

SOKOL_API_IMPL void scam_shutdown()
{
#if !SOKOL_CAM_USE_NDK_TO_GET_CAM_LIST
	ACameraManager_delete(s_camera_manager);
	s_camera_manager = 0;
#endif
}

SOKOL_API_IMPL scam_camera_list_t scam_get_camera_list()
{
#if SOKOL_CAM_USE_NDK_TO_GET_CAM_LIST
	ACameraIdList *camera_id_list;
	camera_status_t status = ACameraManager_getCameraIdList(s_camera_manager, &camera_id_list);

	scam_camera_list_t res = { ._internal_handle = camera_id_list, .err_code = status, };
	const uint32_t num_cameras = (uint32_t)camera_id_list->numCameras;
	SOKOL_ASSERT(num_cameras <= SCAM_MAX_CAMERAS);
	res.ids = camera_id_list->cameraIds;
	for(uint32_t i = 0; i < num_cameras; i++) {
		ACameraMetadata* metadata_obj;
		ACameraManager_getCameraCharacteristics(s_camera_manager, camera_id_list->cameraIds[i], &metadata_obj);

		int32_t num_tags = 0;
		const uint32_t* tags = 0;
		ACameraMetadata_getAllTags(metadata_obj, &num_tags, &tags);
		for(int32_t j = 0; j < num_tags; j++) {
			const uint32_t tag = tags[j];
			if (ACAMERA_LENS_FACING == tag) {
				ACameraMetadata_const_entry lens_info = { 0 };
				ACameraMetadata_getConstEntry(metadata_obj, tag, &lens_info);

				switch (lens_info.data.u8[0]) {
					case ACAMERA_LENS_FACING_BACK:
						res.types[i] = SCAM_CAMERA_TYPE_MAIN;
						break;
					case ACAMERA_LENS_FACING_FRONT:
						res.types[i] = SCAM_CAMERA_TYPE_SELFIE;
						break;
					case ACAMERA_LENS_FACING_EXTERNAL:
						res.types[i] = SCAM_CAMERA_TYPE_EXTERNAL;
						break;
				}
				break;
			}
		}
	}

	return res;
#else
	ANativeActivity *activity = (ANativeActivity *)sapp_android_get_native_activity();
	JavaVM *lJavaVM = activity->vm;
	JNIEnv *lJNIEnv = 0;
	bool lThreadAttached = false;

	// Get JNIEnv from lJavaVM using GetEnv to test whether
	// thread is attached or not to the VM. If not, attach it
	// (and note that it will need to be detached at the end
	//  of the function).
	switch ((*lJavaVM)->GetEnv(lJavaVM, (void **)&lJNIEnv, JNI_VERSION_1_6))
	{
	case JNI_OK:
		break;
	case JNI_EDETACHED:
	{
		jint lResult = (*lJavaVM)->AttachCurrentThread(lJavaVM, &lJNIEnv, 0);
		if (lResult == JNI_ERR)
		{
			// throw std::runtime_error("Could not attach current thread");
		}
		lThreadAttached = true;
	}
	break;
	case JNI_EVERSION:
		// throw std::runtime_error("Invalid java version");
		break;
	}

	jclass class_Integer = (*lJNIEnv)->FindClass(lJNIEnv, "java/lang/Integer");
	jmethodID method_Integer_intValue = (*lJNIEnv)->GetMethodID(lJNIEnv, class_Integer, "intValue", "()I");

	jclass class_Activity = (*lJNIEnv)->FindClass(lJNIEnv, "android/app/Activity");
	jmethodID method_getSystemService = (*lJNIEnv)->GetMethodID(
		lJNIEnv,
		class_Activity, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;"
	);

	jclass class_Context = (*lJNIEnv)->FindClass(lJNIEnv, "android/content/Context");
	jfieldID field_CAMERA_SERVICE = (*lJNIEnv)->GetStaticFieldID(lJNIEnv, class_Context, "CAMERA_SERVICE", "Ljava/lang/String;");
	jobject object_CAMERA_SERVICE = (*lJNIEnv)->GetStaticObjectField(lJNIEnv, class_Context, field_CAMERA_SERVICE);

	jobject object_systemService = (*lJNIEnv)->CallObjectMethod(lJNIEnv, activity->clazz, method_getSystemService, object_CAMERA_SERVICE);
	jclass class_CameraManager = (*lJNIEnv)->FindClass(lJNIEnv, "android/hardware/camera2/CameraManager");
	jmethodID method_getCameraIdList = (*lJNIEnv)->GetMethodID(lJNIEnv,
		class_CameraManager,
		"getCameraIdList", "()[Ljava/lang/String;");
	jmethodID method_getCameraCharacteristics = (*lJNIEnv)->GetMethodID(lJNIEnv,
		class_CameraManager,
		"getCameraCharacteristics", "(Ljava/lang/String;)Landroid/hardware/camera2/CameraCharacteristics;");

	jclass class_CameraCharacteristics = (*lJNIEnv)->FindClass(lJNIEnv, "android/hardware/camera2/CameraCharacteristics");
	// & "C:\Program Files\Java\jdk-20\bin\javap.exe" -classpath C:\Users\tuket\Documents\APPS\android_sdk\platforms\android-24\android.jar -s android.hardware.camera2.CameraCharacteristics
	jmethodID method_CameraCharacteristics_get = (*lJNIEnv)->GetMethodID(lJNIEnv,
		class_CameraCharacteristics, "get", "(Landroid/hardware/camera2/CameraCharacteristics$Key;)Ljava/lang/Object;");
	jfieldID field_CameraCharacteristics_LENS_FACING = (*lJNIEnv)->GetStaticFieldID(lJNIEnv,
		class_CameraCharacteristics, "LENS_FACING", "Landroid/hardware/camera2/CameraCharacteristics$Key;");
	jobject object_CameraCharacteristics_LENS_FACING = (*lJNIEnv)->GetStaticObjectField(lJNIEnv,
		class_CameraCharacteristics, field_CameraCharacteristics_LENS_FACING);

	jfieldID field_CameraCharacteristics_LENS_FACING_FRONT = (*lJNIEnv)->GetStaticFieldID(lJNIEnv,
		class_CameraCharacteristics, "LENS_FACING_FRONT", "I");
	int CameraCharacteristics_LENS_FACING_FRONT = (*lJNIEnv)->GetStaticIntField(lJNIEnv,
		class_CameraCharacteristics, field_CameraCharacteristics_LENS_FACING_FRONT);
	//__android_log_buf_print(0, ANDROID_LOG_DEBUG, "DEBUGGG", "LENS_FACING_FRONT: %d", CameraCharacteristics_LENS_FACING_FRONT);

	jfieldID field_CameraCharacteristics_LENS_FACING_BACK = (*lJNIEnv)->GetStaticFieldID(lJNIEnv,
		class_CameraCharacteristics, "LENS_FACING_BACK", "I");
	int CameraCharacteristics_LENS_FACING_BACK = (*lJNIEnv)->GetStaticIntField(lJNIEnv,
		class_CameraCharacteristics, field_CameraCharacteristics_LENS_FACING_BACK);
	//__android_log_buf_print(0, ANDROID_LOG_DEBUG, "DEBUGGG", "LENS_FACING_BACK: %d", CameraCharacteristics_LENS_FACING_BACK);

	jobject object_cameraList = (*lJNIEnv)->CallObjectMethod(lJNIEnv, object_systemService, method_getCameraIdList);
	jobjectArray array_cameraList = (jobjectArray*)object_cameraList;
	const jsize num_cameras = (*lJNIEnv)->GetArrayLength(lJNIEnv, array_cameraList);

	scam_camera_list_t res = { .num_cameras = num_cameras, };
	if(num_cameras == 0)
		return res;

	size_t totalLen = 0;
	for(jsize i = 0; i < num_cameras; i++) {
		jobject object_str = (*lJNIEnv)->GetObjectArrayElement(lJNIEnv, array_cameraList, i);
		jstring* jstr = (jstring*)&object_str;
		size_t len = (*lJNIEnv)->GetStringLength(lJNIEnv, *jstr);
		totalLen += len + 1;
	}
	void* alloc_ptr = malloc(totalLen + sizeof(char*) * num_cameras);
	res.ids = (const char**)alloc_ptr;
	char* cursor = (char*)alloc_ptr + sizeof(char*) * num_cameras;
	
	for(jsize i = 0; i < num_cameras; i++) {
		jobject object_str = (*lJNIEnv)->GetObjectArrayElement(lJNIEnv, array_cameraList, i);
		jstring* jstr = (jstring*)&object_str;
		size_t len = (*lJNIEnv)->GetStringLength(lJNIEnv, *jstr);
		const char* chars = (*lJNIEnv)->GetStringUTFChars(lJNIEnv, *jstr, NULL);
		memcpy(cursor, chars, len);
		cursor[len] = '\0';
		res.ids[i] = cursor;
		cursor += len + 1;

		jobject object_camCharacteristics = (*lJNIEnv)->CallObjectMethod(lJNIEnv, object_systemService, method_getCameraCharacteristics, object_str);
		jobject facingObj = (*lJNIEnv)->CallObjectMethod(lJNIEnv,
			object_camCharacteristics, method_CameraCharacteristics_get, object_CameraCharacteristics_LENS_FACING);
		int facingI = (*lJNIEnv)->CallIntMethod(lJNIEnv, facingObj, method_Integer_intValue);

		//__android_log_buf_print(0, ANDROID_LOG_DEBUG, "DEBUGGG", "facingI(%d): %d", (int)i, facingI);
		res.types[i] =
			(facingI == CameraCharacteristics_LENS_FACING_FRONT) ? SCAM_CAMERA_TYPE_SELFIE :
			(facingI == CameraCharacteristics_LENS_FACING_BACK) ? SCAM_CAMERA_TYPE_MAIN :
			SCAM_CAMERA_TYPE_EXTERNAL;
		//__android_log_buf_print(0, ANDROID_LOG_DEBUG, "DEBUGGG", "res.types[%d]: %d", (int)i, (int)res.types[i]);
	}
	
	return res;
#endif
}

void scam_delete_camera_list(scam_camera_list_t* l)
{
#if SOKOL_CAM_USE_NDK_TO_GET_CAM_LIST
	ACameraManager_deleteCameraIdList((ACameraIdList*)l->_internal_handle);
#else
	if(l->num_cameras)
		free(l->ids);
#endif
}

#endif

#endif
