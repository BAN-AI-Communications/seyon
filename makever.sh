#!/usr/bin/env sh

# Sometimes hostname is broken and exit with nonzero even if it
# succeeds, so this approach is not used.

#host=`hostname || uname -n || printf '%s\n' host`
#if [ "$USER" != "" ]; then user=$USER
#elif [ "$LOGNAME" != "" ]; then user=$LOGNAME
#elif [ "$LOGNAME" != "" ]; then user=$LOGNAME
#else user="user"
#fi

VERSION="2"
REVISION="20c-jhj"

host=$(hostname)
if [ "$host" = "" ]; then host=$(uname -n); fi
if [ "$host" = "" ]; then host=$HOST; fi
if [ "$host" = "" ]; then host="unknown"; fi

user=$USER
if [ "$user" = "" ]; then user=$LOGNAME; fi
if [ "$user" = "" ]; then user=$(logname); fi
if [ "$user" = "" ]; then user="unknown"; fi

system=$(uname -s)
if [ "$system" = "" ]; then system="unknown"; fi

machine=$(uname -m)
if [ "$machine" = "" ]; then machine=$MACHINE; fi
if [ "$machine" = "" ]; then machine=$HOSTTYPE; fi
if [ "$machine" = "" ]; then machine=$hosttype; fi
if [ "$machine" = "" ]; then machine="unknown"; fi

# This requires bash (ksh and zsh would probably work too), but is not
# supported by the standard sh.

#host=`hostname`; host=${host:-${`uname -n`:-"host"}}
#user=${USER:-${LOGNAME:-${`logname`:-"user"}}}

printf '%s\n' "#define COMPILE_OSNAME \"$system\"" >> version.h
printf '%s\n' "#define COMPILE_HOSTTYPE \"$machine\"" >> version.h

printf '%s\n' "#define VERSION \"$VERSION\"" >> version.h
printf '%s\n' "#define REVISION \"$REVISION\"" >> version.h

#printf '%s\n' "#ifdef IS_MAIN" >> version.h
#printf '%s\n' "static char version[] = \"\$Revision: $VERSION.$REVISION \$\";"\
#     >> version.h
#printf '%s\n' "#endif" >> version.h

printf '%s\n' "Machine type is $machine, OS name is $system"
