/*
 * io.h
 *
 *  Created on: Jul 6, 2016
 *      Author: reboot
 */

#ifndef SRC_IO_H_
#define SRC_IO_H_

int
self_get_path (const char *exec, char *out);
int
find_absolute_path (const char *exec, char *output);
int
get_file_type (char *file);
int
dir_exists (char *dir);
int
file_exists (char *dir);
int
symlink_exists (char *dir);

#endif /* SRC_IO_H_ */
