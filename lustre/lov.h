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
#ifndef LOV_H
#define LOV_H

#include <obd.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/
class LOV : public DataDevice
{
	int num;
	dtvec_t lov;
	Layout layout; /* default Layout configuration */

	int ReadWrite(IO *io);
	obd_count StripeOffset(Layout *lyt, obd_count off);

	static void IoCompletion(IO *io);
public:
	LOV();
	~LOV();

	void Attach(Node *site);
	void Setup();
	void Cleanup();
	int Disconnect();
	int Read(IO *io);
	int Write(IO *io);
	int Create(Object *obj);
	int Destroy(Object *obj);
};

#endif
