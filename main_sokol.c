#define SOKOL_IMPL
#define SOKOL_GLES3
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_app.h"
#include "sokol/sokol_log.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_cam.h"
// #include "dbgui/dbgui.h"

// #define SOKOL_GL_IMPL
#include "sokol/util/sokol_gl.h"

// include nuklear.h before the sokol_nuklear.h implementation
#define NK_IMPLEMENTATION
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_STANDARD_VARARGS
#include "nuklear.h"
// #define SOKOL_NUKLEAR_IMPL
#include "sokol/util/sokol_nuklear.h"

#include <jni.h>

/*
For some reason we n I try to get the list of cameras I always get 0 elements?!
I'm asking for permission and still not woking.
Some links that might help:
https://www.sisik.eu/blog/android/ndk/camera
https://github.dev/makepad/makepad
https://github.dev/sixo/native-camera/tree/master/app/src/main/java/eu/sisik/cam
https://github.com/search?q=ACameraManager_getCameraIdList&type=code
*/

static int android_get_jni_env(JavaVM *lJavaVM, JNIEnv **lJNIEnv)
{
	// Get JNIEnv from lJavaVM using GetEnv to test whether
	// thread is attached or not to the VM. If not, attach it
	// (and note that it will need to be detached at the end
	//  of the function).
	int lThreadAttached = false;
	switch ((*lJavaVM)->GetEnv(lJavaVM, (void **)lJNIEnv, JNI_VERSION_1_6))
	{
	case JNI_OK:
		break;
	case JNI_EDETACHED:
	{
		jint lResult = (*lJavaVM)->AttachCurrentThread(lJavaVM, lJNIEnv, 0);
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
	return lThreadAttached;
}

static jstring android_permission_name(JNIEnv *lJNIEnv, const char *perm_name)
{
	// nested class permission in class android.Manifest,
	// hence android 'slash' Manifest 'dollar' permission
	jclass ClassManifestpermission = (*lJNIEnv)->FindClass(
		lJNIEnv,
		"android/Manifest$permission");
	jfieldID lid_PERM = (*lJNIEnv)->GetStaticFieldID(
		lJNIEnv,
		ClassManifestpermission, perm_name, "Ljava/lang/String;");
	jstring ls_PERM = (jstring)((*lJNIEnv)->GetStaticObjectField(
		lJNIEnv,
		ClassManifestpermission, lid_PERM));
	return ls_PERM;
}

/**
 * \brief Tests whether a permission is granted.
 * \param[in] app a pointer to the android app.
 * \param[in] perm_name the name of the permission, e.g.,
 *   "READ_EXTERNAL_STORAGE", "WRITE_EXTERNAL_STORAGE".
 * \retval true if the permission is granted.
 * \retval false otherwise.
 * \note Requires Android API level 23 (Marshmallow, May 2015)
 */
static int android_has_permission(ANativeActivity *activity, const char *perm_name)
{
	JavaVM *lJavaVM = activity->vm;
	JNIEnv *lJNIEnv = 0;
	bool lThreadAttached = android_get_jni_env(lJavaVM, &lJNIEnv);

	int result = 0;

	jstring ls_PERM = android_permission_name(lJNIEnv, perm_name);

	jint PERMISSION_GRANTED = (jint)-1;
	{
		jclass ClassPackageManager = (*lJNIEnv)->FindClass(
			lJNIEnv,
			"android/content/pm/PackageManager");
		jfieldID lid_PERMISSION_GRANTED = (*lJNIEnv)->GetStaticFieldID(
			lJNIEnv,
			ClassPackageManager, "PERMISSION_GRANTED", "I");
		PERMISSION_GRANTED = (*lJNIEnv)->GetStaticIntField(
			lJNIEnv,
			ClassPackageManager, lid_PERMISSION_GRANTED);
	}
	{
		jobject activityObj = activity->clazz;
		jclass ClassContext = (*lJNIEnv)->FindClass(
			lJNIEnv,
			"android/content/Context");
		jmethodID MethodcheckSelfPermission = (*lJNIEnv)->GetMethodID(
			lJNIEnv,
			ClassContext, "checkSelfPermission", "(Ljava/lang/String;)I");
		jint int_result = (*lJNIEnv)->CallIntMethod(
			lJNIEnv,
			activityObj, MethodcheckSelfPermission, ls_PERM);
		result = (int_result == PERMISSION_GRANTED);
	}

	if (lThreadAttached)
		(*lJavaVM)->DetachCurrentThread(lJavaVM);

	return result;
}

static void android_request_permissions(ANativeActivity *activity, uint32_t num_permissions, const char **permissions_names)
{
	JavaVM *lJavaVM = activity->vm;
	JNIEnv *lJNIEnv = 0;
	int lThreadAttached = android_get_jni_env(lJavaVM, &lJNIEnv);

	jobjectArray perm_array = (*lJNIEnv)->NewObjectArray(
		lJNIEnv,
		num_permissions,
		(*lJNIEnv)->FindClass(lJNIEnv, "java/lang/String"),
		(*lJNIEnv)->NewStringUTF(lJNIEnv, ""));

	for (uint32_t i = 0; i < num_permissions; i++)
	{
		(*lJNIEnv)->SetObjectArrayElement(
			lJNIEnv,
			perm_array, i,
			android_permission_name(lJNIEnv, permissions_names[i]));
	}

	jobject activityObj = activity->clazz;

	jclass ClassActivity = (*lJNIEnv)->FindClass(
		lJNIEnv,
		"android/app/Activity");

	jmethodID MethodrequestPermissions = (*lJNIEnv)->GetMethodID(
		lJNIEnv,
		ClassActivity, "requestPermissions", "([Ljava/lang/String;I)V");

	// Last arg (0) is just for the callback (that I do not use)
	(*lJNIEnv)->CallVoidMethod(
		lJNIEnv,
		activityObj, MethodrequestPermissions, perm_array, 0);

	if (lThreadAttached)
		(*lJavaVM)->DetachCurrentThread(lJavaVM);
}

/**
 * \brief Query file permissions.
 * \details This opens the system dialog that lets the user
 *  grant (or deny) the permission.
 * \param[in] app a pointer to the android app.
 * \note Requires Android API level 23 (Marshmallow, May 2015)
 */
static void android_request_file_permissions(ANativeActivity *activity)
{
	const char* permissions_to_request[] = {"READ_EXTERNAL_STORAGE", "WRITE_EXTERNAL_STORAGE"};
	android_request_permissions(activity, 2, permissions_to_request);
}

void check_android_permissions(ANativeActivity *activity)
{
	bool OK = android_has_permission(
				  activity, "READ_EXTERNAL_STORAGE") &&
			  android_has_permission(
				  activity, "WRITE_EXTERNAL_STORAGE");
	if (!OK)
	{
		android_request_file_permissions(activity);
	}
}

#define OFFSCREEN_WIDTH (32)
#define OFFSCREEN_HEIGHT (32)
#define OFFSCREEN_COLOR_FORMAT (SG_PIXELFORMAT_RGBA8)
#define OFFSCREEN_DEPTH_FORMAT (SG_PIXELFORMAT_DEPTH)
#define OFFSCREEN_SAMPLE_COUNT (1)

static struct
{
	double angle_deg;
	struct
	{
		sgl_context sgl_ctx;
		sgl_pipeline sgl_pip;
		sg_image color_img;
		sg_image depth_img;
		sg_pass pass;
		sg_pass_action pass_action;
	} offscreen;
	struct
	{
		sg_pass_action pass_action;
	} display;
	struct
	{
		snk_image_t img_nearest_clamp;
		snk_image_t img_linear_clamp;
		snk_image_t img_nearest_repeat;
		snk_image_t img_linear_mirror;
	} ui;
	scam_camera_list_t cam_list;
} state;

static void draw_cube(void);

static void init(void)
{
	sg_setup(&(sg_desc){
		.context = sapp_sgcontext(),
		.logger.func = slog_func,
	});
	//__dbgui_setup(sapp_sample_count());
	snk_setup(&(snk_desc_t){
		.dpi_scale = 3 * sapp_dpi_scale(),
		.logger.func = slog_func,
	});
	sgl_setup(&(sgl_desc_t){
		.logger.func = slog_func,
	});

	scam_setup();

	// a sokol-gl context to render into an offscreen pass
	state.offscreen.sgl_ctx = sgl_make_context(&(sgl_context_desc_t){
		.color_format = OFFSCREEN_COLOR_FORMAT,
		.depth_format = OFFSCREEN_DEPTH_FORMAT,
		.sample_count = OFFSCREEN_SAMPLE_COUNT,
	});

	// a sokol-gl pipeline object for 3D rendering
	state.offscreen.sgl_pip = sgl_context_make_pipeline(state.offscreen.sgl_ctx, &(sg_pipeline_desc){
		.cull_mode = SG_CULLMODE_BACK,
		.depth = {
			.write_enabled = true,
			.pixel_format = OFFSCREEN_DEPTH_FORMAT,
			.compare = SG_COMPAREFUNC_LESS_EQUAL,
		},
		.colors[0].pixel_format = OFFSCREEN_COLOR_FORMAT,
		.sample_count = OFFSCREEN_SAMPLE_COUNT,
	});

	// color and depth render target textures for the offscreen pass
	state.offscreen.color_img = sg_make_image(&(sg_image_desc){
		.render_target = true,
		.width = OFFSCREEN_WIDTH,
		.height = OFFSCREEN_HEIGHT,
		.pixel_format = OFFSCREEN_COLOR_FORMAT,
		.sample_count = OFFSCREEN_SAMPLE_COUNT,
	});
	state.offscreen.depth_img = sg_make_image(&(sg_image_desc){
		.render_target = true,
		.width = OFFSCREEN_WIDTH,
		.height = OFFSCREEN_HEIGHT,
		.pixel_format = OFFSCREEN_DEPTH_FORMAT,
		.sample_count = OFFSCREEN_SAMPLE_COUNT,
	});

	// a render pass object to render into the offscreen render targets
	state.offscreen.pass = sg_make_pass(&(sg_pass_desc){
		.color_attachments[0].image = state.offscreen.color_img,
		.depth_stencil_attachment.image = state.offscreen.depth_img,
	});

	// a pass-action for the offscreen pass which clears to black
	state.offscreen.pass_action = (sg_pass_action){
		.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}},
	};

	// a pass action for the default pass which clears to blue-ish
	state.display.pass_action = (sg_pass_action){
		.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.5f, 0.5f, 1.0f, 1.0f}},
	};

	// sokol-nuklear image-sampler-pair wrappers which combine the offscreen
	// render target texture with different sampler types
	state.ui.img_nearest_clamp = snk_make_image(&(snk_image_desc_t){
		.image = state.offscreen.color_img,
		.sampler = sg_make_sampler(&(sg_sampler_desc){
			.min_filter = SG_FILTER_NEAREST,
			.mag_filter = SG_FILTER_NEAREST,
			.wrap_u = SG_WRAP_CLAMP_TO_EDGE,
			.wrap_v = SG_WRAP_CLAMP_TO_EDGE,
		})});
	state.ui.img_linear_clamp = snk_make_image(&(snk_image_desc_t){
		.image = state.offscreen.color_img,
		.sampler = sg_make_sampler(&(sg_sampler_desc){
			.min_filter = SG_FILTER_LINEAR,
			.mag_filter = SG_FILTER_LINEAR,
			.wrap_u = SG_WRAP_CLAMP_TO_EDGE,
			.wrap_v = SG_WRAP_CLAMP_TO_EDGE,
		})});
	state.ui.img_nearest_repeat = snk_make_image(&(snk_image_desc_t){
		.image = state.offscreen.color_img,
		.sampler = sg_make_sampler(&(sg_sampler_desc){
			.min_filter = SG_FILTER_NEAREST,
			.mag_filter = SG_FILTER_NEAREST,
			.wrap_u = SG_WRAP_REPEAT,
			.wrap_v = SG_WRAP_REPEAT,
		})});
	state.ui.img_linear_mirror = snk_make_image(&(snk_image_desc_t){
		.image = state.offscreen.color_img,
		.sampler = sg_make_sampler(&(sg_sampler_desc){
			.min_filter = SG_FILTER_LINEAR,
			.mag_filter = SG_FILTER_LINEAR,
			.wrap_u = SG_WRAP_MIRRORED_REPEAT,
			.wrap_v = SG_WRAP_MIRRORED_REPEAT,
		})});
}

static void frame(void)
{
	state.angle_deg += sapp_frame_duration() * 60.0;
	const float a = sgl_rad((float)state.angle_deg);

	// first use sokol-gl to render a rotating cube into the sokol-gl context,
	// this just records draw command for the later sokol-gfx render pass
	sgl_set_context(state.offscreen.sgl_ctx);
	sgl_defaults();
	sgl_load_pipeline(state.offscreen.sgl_pip);
	sgl_matrix_mode_projection();
	sgl_perspective(sgl_rad(45.0f), 1.0f, 0.1f, 100.0f);
	const float eye[3] = {sinf(a) * 4.0f, sinf(a) * 2.0f, cosf(a) * 4.0f};
	sgl_matrix_mode_modelview();
	sgl_lookat(eye[0], eye[1], eye[2], 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	draw_cube();

	// specific the Nuklear UI (this also just records draw commands which
	// are then rendered later in the frame in the sokol-gfx default pass)
	struct nk_context *ctx = snk_new_frame();
	if (nk_begin(ctx, "Sokol + Nuklear Image Test", nk_rect(0, 0, sapp_widthf(), sapp_heightf()), NK_WINDOW_BORDER | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE | NK_WINDOW_MINIMIZABLE))
	{
		char buffer[256];
		ANativeActivity *activity = (ANativeActivity *)sapp_android_get_native_activity();
		if (activity)
		{
			const int have_write_permission = android_has_permission(activity, "WRITE_EXTERNAL_STORAGE");
			if(!have_write_permission) {
				const char* permissions_to_request[] = {"WRITE_EXTERNAL_STORAGE"};
				android_request_permissions(activity, 1, permissions_to_request);
			}

			const int have_cam_permission = android_has_permission(activity, "CAMERA");
			snprintf(buffer, 256, "have cam premission: %d", have_cam_permission);
			nk_layout_row_dynamic(ctx, 0, 1);
			nk_label(ctx, buffer, NK_TEXT_LEFT);

			if (have_cam_permission)
			{
				if (state.cam_list.num_cameras == 0)
					state.cam_list = scam_get_camera_list();
			}
			else
			{
				const char *permissions_to_request[] = {"CAMERA"};
				android_request_permissions(activity, 1, permissions_to_request);
			}

		}
		else
		{
			nk_layout_row_dynamic(ctx, 0, 1);
			nk_label(ctx, "NULL ACTIVITY?", NK_TEXT_LEFT);
		}

		snprintf(buffer, 256, "num cameras: %d", (int)state.cam_list.num_cameras);
		nk_layout_row_dynamic(ctx, 0, 1);
		nk_label(ctx, buffer, NK_TEXT_LEFT);

		snprintf(buffer, 256, "err_code: %d", (int)state.cam_list.err_code);
		nk_layout_row_dynamic(ctx, 0, 1);
		nk_label(ctx, buffer, NK_TEXT_LEFT);

		for (uint32_t i = 0; i < state.cam_list.num_cameras; i++)
		{
			scam_camera_type type = state.cam_list.types[i];
			const char *id = state.cam_list.ids[i];
			const char *type_str =
				type == SCAM_CAMERA_TYPE_MAIN ? "MAIN" :
				type == SCAM_CAMERA_TYPE_SELFIE ? "SELFIE" : "EXTERNAL";
			snprintf(buffer, 256, "%s, %s", id, type_str);
			nk_layout_row_dynamic(ctx, 0, 1);
			nk_label(ctx, buffer, NK_TEXT_LEFT);
		}
	}
	nk_end(ctx);

	// perform sokol-gfx rendering...
	// ...first the offscreen pass which renders the sokol-gl scene
	sg_begin_pass(state.offscreen.pass, &state.offscreen.pass_action);
	sgl_context_draw(state.offscreen.sgl_ctx);
	sg_end_pass();

	// then the display pass with the Dear ImGui scene
	sg_begin_default_pass(&state.display.pass_action, sapp_width(), sapp_height());
	snk_render(sapp_width(), sapp_height());
	//__dbgui_draw();
	sg_end_pass();
	sg_commit();
}

static void input(const sapp_event *ev)
{
	/*if (__dbgui_event_with_retval(ev)) {
		return;
	}*/
	snk_handle_event(ev);
}

static void cleanup(void)
{
	sgl_shutdown();
	snk_shutdown();
	sg_shutdown();
	scam_shutdown();
}

sapp_desc sokol_main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	return (sapp_desc){
		.init_cb = init,
		.frame_cb = frame,
		.cleanup_cb = cleanup,
		.event_cb = input,
		.enable_clipboard = true,
		.width = 0,
		.height = 0,
		.high_dpi = 1,
		.window_title = "test",
		.icon.sokol_default = true,
		.logger.func = slog_func,
	};
}

// vertex specification for a cube with colored sides and texture coords
static void draw_cube(void)
{
	sgl_begin_quads();
	sgl_c3f(1.0f, 0.0f, 0.0f);
	sgl_v3f_t2f(-1.0f, 1.0f, -1.0f, -1.0f, 1.0f);
	sgl_v3f_t2f(1.0f, 1.0f, -1.0f, 1.0f, 1.0f);
	sgl_v3f_t2f(1.0f, -1.0f, -1.0f, 1.0f, -1.0f);
	sgl_v3f_t2f(-1.0f, -1.0f, -1.0f, -1.0f, -1.0f);
	sgl_c3f(0.0f, 1.0f, 0.0f);
	sgl_v3f_t2f(-1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
	sgl_v3f_t2f(1.0f, -1.0f, 1.0f, 1.0f, 1.0f);
	sgl_v3f_t2f(1.0f, 1.0f, 1.0f, 1.0f, -1.0f);
	sgl_v3f_t2f(-1.0f, 1.0f, 1.0f, -1.0f, -1.0f);
	sgl_c3f(0.0f, 0.0f, 1.0f);
	sgl_v3f_t2f(-1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
	sgl_v3f_t2f(-1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	sgl_v3f_t2f(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f);
	sgl_v3f_t2f(-1.0f, -1.0f, -1.0f, -1.0f, -1.0f);
	sgl_c3f(1.0f, 0.5f, 0.0f);
	sgl_v3f_t2f(1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
	sgl_v3f_t2f(1.0f, -1.0f, -1.0f, 1.0f, 1.0f);
	sgl_v3f_t2f(1.0f, 1.0f, -1.0f, 1.0f, -1.0f);
	sgl_v3f_t2f(1.0f, 1.0f, 1.0f, -1.0f, -1.0f);
	sgl_c3f(0.0f, 0.5f, 1.0f);
	sgl_v3f_t2f(1.0f, -1.0f, -1.0f, -1.0f, 1.0f);
	sgl_v3f_t2f(1.0f, -1.0f, 1.0f, 1.0f, 1.0f);
	sgl_v3f_t2f(-1.0f, -1.0f, 1.0f, 1.0f, -1.0f);
	sgl_v3f_t2f(-1.0f, -1.0f, -1.0f, -1.0f, -1.0f);
	sgl_c3f(1.0f, 0.0f, 0.5f);
	sgl_v3f_t2f(-1.0f, 1.0f, -1.0f, -1.0f, 1.0f);
	sgl_v3f_t2f(-1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	sgl_v3f_t2f(1.0f, 1.0f, 1.0f, 1.0f, -1.0f);
	sgl_v3f_t2f(1.0f, 1.0f, -1.0f, -1.0f, -1.0f);
	sgl_end();
}