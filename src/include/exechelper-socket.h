/* 
    2015 (c) Steve Dodier-Lazaro <sidnioulz@gmail.com>
    This file is part of ExecHelper.

    ExecHelper is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    ExecHelper is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with ExecHelper.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __EH_EXECHELP_SOCKET_H__
#define __EH_EXECHELP_SOCKET_H__

#include "exechelper.h"

void exechelp_build_run_dir(void);
char *exechelp_make_socket(const char *socket_name, const uid_t uid, const gid_t gid, const pid_t pid);

#endif /* __EH_EXECHELP_SOCKET_H__ */
