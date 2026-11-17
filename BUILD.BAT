
@echo off

SET EXE=SLED.EXE
SET SRC=sled.c
SET STK=4096
SET SCM=SCM

IF "%1" == "" GOTO msc
IF "%1" == "MSC" GOTO msc
IF "%1" == "OWC" GOTO owc
IF "%1" == "TESTS" GOTO tests
IF "%1" == "BENCH" GOTO bench
echo Invalid argument: %1 use MSC or OWC
GOTO end

:msc
cl /AS /Fe%EXE% /FPa /W2 /Ox /Zp2 /G2 /D%SCM% %SRC% /link /NOE /STACK:%STK% > BUILD.LOG
dir %EXE% >> BUILD.LOG
GOTO end

:owc
wcl /ms /fe=%EXE% /l=dos /w2 /ox /zp=2 /2 /d%SCM% %SRC% /k%STK% > BUILD.LOG
dir %EXE% >> BUILD.LOG
GOTO end

:tests
%EXE% /I demo\tests.scm > TESTS.LOG
GOTO end

:bench
time > BENCH.LOG
%EXE% /B demo\tak.scm >> BENCH.LOG
time >> BENCH.LOG
GOTO end

:end
SET EXE=
SET SRC=
SET STK=
SET SCM=
