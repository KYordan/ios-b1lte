/* SPDX-License-Identifier: BSD-2-Clause
 * B1L-0024: the 50 removable system apps become plain system apps, as HakiOS
 * does on the S9 (Scion-Project/hackios-xnu-kernel: sysimage/select-system.py,
 * userland/build.sh, userland/src/init.c flash_root). iOS's own route
 * (SystemAppMigrator -> installcoordinationd -> installd) needs APFS staging,
 * containers and placeholder promises, and never completed here.
 *
 * Once per root, before launchd, on the same volume (renames only, nothing is
 * copied or deleted):
 *   /private/var/staged_system_apps/X.app  -> /Applications/X.app
 *     (else /private/var/b2l-parked-0018/Applications/X.app, B1L-0025)
 *   /System/Library/AppPlaceholders/X.app  -> /System/Library/AppPlaceholders.b2l-parked-0024/X.app
 *   LaunchServices stores of lsd's container -> *.b2l-before-0024 (rebuilt by
 *   lsd from /Applications at the next start)
 * A target that already exists is never replaced. The marker
 * /private/var/db/b2l-apps-layout-v1 is written when every app is in place;
 * while the layout is active the installer launcher job is not published.
 * Kill switch: boot-arg b2l_apps_layout=0 (then the v4 launcher runs again).
 */
#ifndef B2L_APPS_LAYOUT_H
#define B2L_APPS_LAYOUT_H

extern int b2l_kernel_rename_excl(const char *from, const char *to, vfs_context_t ctx);

static const char *const b2l_layout_apps[] = {
	"AegirProxyApp.app", "AppStore.app", "AppleTV.app", "AppleVisionProApp.app", "Books.app",
	"Bridge.app", "Calculator.app", "Camera.app", "Compass.app", "Contacts.app",
	"EmojiPoster.app", "FaceTime.app", "Files.app", "FindMy.app", "Fitness.app",
	"Freeform.app", "Games.app", "GradientPoster.app", "Health.app", "Home.app",
	"Image Playground.app", "Journal.app", "KaleidoscopePosterApp.app", "Magnifier.app", "Maps.app",
	"Measure.app", "MobileCal.app", "MobileMail.app", "MobileNotes.app", "MobileSMS.app",
	"MobileSafari.app", "MobileStore.app", "MobileTimer.app", "Music.app", "News.app",
	"Passbook.app", "Passwords.app", "Photos.app", "Podcasts.app", "Preview.app",
	"PridePosterApp.app", "Reminders.app", "SequoiaTranslator.app", "Shortcuts.app", "Stocks.app",
	"Tips.app", "UnityPosterApp.app", "VoiceMemos.app", "Weather.app", "WeatherPosterApp.app",
};
#define B2L_LAYOUT_MARKER "/private/var/db/b2l-apps-layout-v1"
#define B2L_LAYOUT_LS_DIR "/private/var/mobile/Containers/Data/InternalDaemon/5FD6E102-CC72-4B15-B3CE-51E484BDF8B9/Library/Caches/"

static int
b2l_layout_exists(const char *path, vfs_context_t ctx)
{
	vnode_t vp = NULL;
	int error = vnode_lookup(path, VNODE_LOOKUP_NOFOLLOW_ANY, &vp, ctx);
	if (error == 0) {
		vnode_put(vp);
		return 1;
	}
	return error == ENOENT ? 0 : -error;
}

/* Rename when the source exists and the target does not. 0 moved, 1 nothing to do, <0 error. */
static int
b2l_layout_move(const char *from, const char *to, vfs_context_t ctx)
{
	int src = b2l_layout_exists(from, ctx), dst, error;

	if (src <= 0) {
		return src < 0 ? src : 1;
	}
	dst = b2l_layout_exists(to, ctx);
	if (dst != 0) {
		return dst < 0 ? dst : 1;
	}
	error = b2l_kernel_rename_excl(from, to, ctx);
	return error ? -error : 0;
}

static int
b2l_layout_join(char *out, size_t cap, const char *a, const char *b, const char *c)
{
	return (size_t)snprintf(out, cap, "%s%s%s", a, b, c ? c : "") < cap ? 0 : ENAMETOOLONG;
}

static int
b2l_layout_marker(const char *path, vfs_context_t ctx)
{
	vnode_t vp = NULL;
	int error = b2l_apps_directory("/private/var/db", ctx);
	if (!error) {
		error = vnode_open(path, FREAD | FWRITE | O_CREAT, 0644, VNODE_LOOKUP_NOFOLLOW_ANY, &vp, ctx);
	}
	if (!error) {
		error = VNOP_FSYNC(vp, MNT_WAIT, ctx);
		int ce = vnode_close(vp, FREAD | FWRITE, ctx);
		error = error ? error : ce;
	}
	return error;
}

/*
 * B1L-0026: Control Center opened empty. Its icon state
 * (ControlsIconState.plist) was written with no controls, before the apps and
 * the controls extension existed. Once, both state files are renamed so that
 * ControlCenterUI builds its default layout again. Kill switch: b2l_cc_reset=0.
 */
#define B2L_CC_MARKER "/private/var/db/b2l-cc-reset-v2"
static void
b2l_cc_layout_reset(vfs_context_t ctx)
{
	static const char *const names[] = { "ControlsIconState.plist", "ControlsMigrationSettings.plist" };
	static char from[MAXPATHLEN], to[MAXPATHLEN];
	int on = 1, r, error;
	unsigned i, renamed = 0;

	PE_parse_boot_argn("b2l_cc_reset", &on, sizeof(on));
	if (!on || b2l_layout_exists(B2L_CC_MARKER, ctx) == 1) {
		return;
	}
	for (i = 0; i < 2; i++) {
		if (b2l_layout_join(from, sizeof(from), "/private/var/mobile/Library/ControlCenter/", names[i], NULL) ||
		    b2l_layout_join(to, sizeof(to), "/private/var/mobile/Library/ControlCenter/", names[i], ".b2l-before-0027")) {
			continue;
		}
		r = b2l_layout_move(from, to, ctx);
		renamed += r == 0;
		printf("B2L CC: %s %s\n", names[i], r == 0 ? "renamed" : r == 1 ? "absent/kept" : "error");
	}
	error = b2l_layout_marker(B2L_CC_MARKER, ctx);
	printf("B2L CC: layout reset renamed=%u marker error=%d\n", renamed, error);
}

/* 1: layout active (launcher stays off); 0: disabled or incomplete. */
static int
b2l_apps_layout(vfs_context_t ctx)
{
	static char from[MAXPATHLEN], to[MAXPATHLEN];
	unsigned moved = 0, placed = 0, parked = 0, failed = 0, i;
	int on = 1, r, error;

	PE_parse_boot_argn("b2l_apps_layout", &on, sizeof(on));
	if (!on) {
		printf("B2L LAYOUT: disabled by boot-arg\n");
		return 0;
	}
	if (b2l_layout_exists(B2L_LAYOUT_MARKER, ctx) == 1) {
		printf("B2L LAYOUT: already done (%s)\n", B2L_LAYOUT_MARKER);
		return 1;
	}
	error = b2l_apps_directory("/System/Library/AppPlaceholders.b2l-parked-0024", ctx);
	if (error) {
		printf("B2L LAYOUT: parking directory error=%d\n", error);
		return 0;
	}
	for (i = 0; i < sizeof(b2l_layout_apps) / sizeof(b2l_layout_apps[0]); i++) {
		const char *n = b2l_layout_apps[i];

		if (b2l_layout_join(from, sizeof(from), "/private/var/staged_system_apps/", n, NULL) ||
		    b2l_layout_join(to, sizeof(to), "/Applications/", n, NULL)) {
			failed++;
			continue;
		}
		r = b2l_layout_move(from, to, ctx);
		if (r == 1 && b2l_layout_exists(to, ctx) == 0 &&
		    !b2l_layout_join(from, sizeof(from), "/private/var/b2l-parked-0018/Applications/", n, NULL)) {
			/* B1L-0025: iOS's migrator emptied staged_system_apps on an
			 * earlier attempt; the B1L-0013 copies (translated) remain here. */
			r = b2l_layout_move(from, to, ctx);
		}
		if (r == 0) {
			moved++;
		} else if (r < 0) {
			failed++;
			printf("B2L LAYOUT: %s -> /Applications error=%d\n", n, -r);
		}
		if (b2l_layout_exists(to, ctx) == 1) {
			placed++;
		}
		if (!b2l_layout_join(from, sizeof(from), "/System/Library/AppPlaceholders/", n, NULL) &&
		    !b2l_layout_join(to, sizeof(to), "/System/Library/AppPlaceholders.b2l-parked-0024/", n, NULL)) {
			r = b2l_layout_move(from, to, ctx);
			if (r == 0) {
				parked++;
			} else if (r < 0) {
				failed++;
				printf("B2L LAYOUT: placeholder %s error=%d\n", n, -r);
			}
		}
	}
	static const char *const stores[] = {
		"com.apple.LaunchServices-20971545-v2.csstore",
		"SystemDataOnly-com.apple.LaunchServices-20971545-v2.csstore",
	};
	for (i = 0; i < 2; i++) {
		if (!b2l_layout_join(from, sizeof(from), B2L_LAYOUT_LS_DIR, stores[i], NULL) &&
		    !b2l_layout_join(to, sizeof(to), B2L_LAYOUT_LS_DIR, stores[i], ".b2l-before-0025")) {
			r = b2l_layout_move(from, to, ctx);
			printf("B2L LAYOUT: %s %s\n", stores[i], r == 0 ? "renamed" : r == 1 ? "absent/kept" : "error");
		}
	}
	error = VFS_SYNC(vnode_mount(rootvnode), MNT_WAIT, ctx);
	printf("B2L LAYOUT: apps moved=%u in_place=%u/%lu placeholders_parked=%u failed=%u sync=%d\n",
	    moved, placed, sizeof(b2l_layout_apps) / sizeof(b2l_layout_apps[0]), parked, failed, error);
	if (placed != sizeof(b2l_layout_apps) / sizeof(b2l_layout_apps[0]) || failed || error) {
		return 0;
	}
	error = b2l_layout_marker(B2L_LAYOUT_MARKER, ctx);
	printf("B2L LAYOUT: marker error=%d\n", error);
	return 1;
}
#endif
