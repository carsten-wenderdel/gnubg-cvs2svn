/*
 * pythonmodule.h
 *
 * by Joern Thyssen <jth@gnubg.org>, 2003
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id$
 */

#ifndef _PYTHONMODULE_H_
#define _PYTHONMODULE_H_

#if HAVE_CONFIG_H
#include "config.h"
#if USE_PYTHON
#undef HAVE_FSTAT
#endif
#endif

#if USE_PYTHON
#include <Python.h>
#if HAVE_CONFIG_H
#undef HAVE_FSTAT
#include "config.h"
#endif

extern PyMethodDef gnubgMethods[];

extern void
PythonInitialise( const char *arg0, const char *szDir );

extern void
PythonShutdown();

#endif /* USE_PYTHON */

#endif /* _PYTHONMODULE_H_ */
