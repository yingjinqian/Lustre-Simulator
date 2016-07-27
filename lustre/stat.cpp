/***************************************************************************
 *   Copyright (C) 2008 by qian                                            *
 *                                                                         *
 *   Storage Team in NUDT                                                  *
 *   Yingjin Qian <yingjin.qian@sun.com>                                   *
 *                                                                         *
 *   This file is part of Lustre, http://www.lustre.org                    *
 *                                                                         *
 *   Lustre is free software; you can redistribute it and/or               *
 *   modify it under the terms of version 2 of the GNU General Public      *
 *   License as published by the Free Software Foundation.                 *
 *                                                                         *
 *   Lustre is distributed in the hope that it will be useful,             *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Lustre; if not, write to the Free Software                 *
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
 ***************************************************************************/
#include "stat.h"
#include <string.h>

Stat::Stat()
{
	cnt = 0;
    file = NULL;
}

Stat::Stat(const char *s)
{
	cnt = 0;
	Init(s);
}

Stat::Stat(FILE *f)
{
    file = f;
}

Stat::~Stat()
{
	if (file && file != stdin && file != stdout)
		fclose(file);
}

int Stat::Init(const char *s)
{
	strncpy(name, s, MAX_NAME_LEN);
	
	file = fopen(name, "w");
	if (file == NULL) {
		printf("Can't open file %s: %s\n",
		       name, strerror(errno));
		return -ENOENT;
	}
	return 0;
}

void Stat::Record(const char *fmt...)
{
	va_list args;
	
	va_start(args,fmt);
	
	assert(file != NULL);
	vfprintf(file,fmt, args);
	va_end(args);
}

#define MAX_LINE_LEN    1024
void Stat::Output(const char *name)
{
    FILE *fp;
    char line[MAX_LINE_LEN];

    if ((fp = fopen(name, "r")) == NULL) {
        printf("Failed to open the file");
        exit(1);
    }

    while (!feof(fp)) {
        fgets(line, MAX_LINE_LEN, fp);
        printf("%s\n", line);
    }
    fclose(fp);
}
