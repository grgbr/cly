.. SPDX-License-Identifier: GPL-3.0-only

   This file is part of Cly
   Copyright (C) 2017-2026 Grégor Boirie <gregor.boirie@free.fr>

TODO
####

#. edition

   #. set completion should generate warning when edition off

#. set command

   #. fix completion for multiple attributes
   #. bits schema leaves support
   #. listleaf schema leaves support

#. enhance completion support

   #. rationalize completion character handling
   #. enhance directory completion logic: disable completion when a single
      argument is expected and one directory has already been completed in
      the current command line.

#. enhance list support

    #. add command support
    #. del command support
    #. add list entry selection argument support

#. pager support

#. safe coloring

#. command line options

   #. logging support
   #. enable / disable libyang cly presentation layer plugin

#. tabular view:

   #. listleaf schema leaves support
   #. header repeat when not paged
   #. render human readable informations
   #. model data groups ?

#. command tree browsing

   #. make commands callable from any node in tree
   #. support libyang cly presentation layer plugin

#. show edition mode marker within prompt

#. user / group nacm privileges support

#. refine / fix process handling:

   #. fix SIGTSTP signal handling
   #. inhibit SIGTSTP when in "login shell mode" ?
   #. backgroup process support ?
   #. process session support ?

#. testing

   #. fuzz
   #. valgrind
   #. gcc sanitization

#. documentation
