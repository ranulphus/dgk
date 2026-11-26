/* dgk/path.h - a file's path on this platform, so a game needs no #ifdef for it. A game names each file once,
 * as DOS would (8.3 letters; DOS's own files are found in any case):
 *
 *   dgk_path(DGK_DATA, "WORLD.PAK")   the game's own data: on DOS the name, beside the program (the current
 *                                     directory, as the game is started); elsewhere in the data directory
 *                                     (dgk_path_dir; DGK_DATA_DIR in the environment wins), the name as given
 *   dgk_path(DGK_USER, "GAME.CFG")    what the game writes (settings, saves): on DOS the name; elsewhere in
 *                                     lower case, in the user directory (dgk_path_dir; DGK_USER_DIR wins)
 *
 * Both directories start as "." (the current one). The path is kept for the program's life. */
#ifndef DGK_PATH_H
#define DGK_PATH_H

enum { DGK_DATA, DGK_USER };

void dgk_path_dir(int kind, const char *dir);        /* off DOS: where that kind of file is (DOS: ignored) */
const char *dgk_path(int kind, const char *name);

#endif
