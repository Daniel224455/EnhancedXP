@REM  -----------------------------------------------------------------
@REM
@REM  signtheme.cmd - ScottHan
@REM     Add the Visual Style signature which will allow us to release 
@REM     visual styles later.
@REM
@REM  Copyright (c) Microsoft Corporation. All rights reserved.
@REM
@REM  -----------------------------------------------------------------
@if defined _CPCMAGIC goto CPCBegin
@perl -x "%~f0" %*
@goto :EOF
#!perl
use strict;
use lib $ENV{RAZZLETOOLPATH} . "\\PostBuildScripts";
use lib $ENV{RAZZLETOOLPATH};
use PbuildEnv;
use ParseArgs;

sub Usage { print<<USAGE; exit(1) }
usage: signtheme.cmd [-l:lang]
    Signs theme files
USAGE

parseargs('?' => \&Usage);


#             *** TEMPLATE CODE ***
$ENV{"_CPCMAGIC"}++;exit(system($0)>>8);
__END__
@:CPCBegin
@set _CPCMAGIC=
@setlocal ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS
@if not defined DEBUG echo off
@REM          *** CMD SCRIPT BELOW ***

rem Visual Styles WERE only available on x86... Now they're for everyone haha. Live with it.
call ExecuteCmd.cmd "packthem.exe -p -q %_NTPOSTBLD%\luna.mst"
if errorlevel == 1 (
	date /t >> %_NTPOSTBLD%\packthem_parse_error.log
	time /t >> %_NTPOSTBLD%\packthem_parse_error.log
	packthem.exe -p -q %_NTPOSTBLD%\luna.mst >> %_NTPOSTBLD%\packthem_parse_error.log
	del %_NTPOSTBLD%\luna.mst.parse_error
	ren %_NTPOSTBLD%\luna.mst luna.mst.parse_error
)
call ExecuteCmd.cmd "packthem.exe -s %_NTPOSTBLD%\luna.mst"
call ExecuteCmd.cmd "checkfix.exe %_NTPOSTBLD%\luna.mst"
