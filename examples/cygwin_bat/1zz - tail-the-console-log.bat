@REM Set the console / terminal to 90 columns, 50 lines
@REM mode con: cols=90 lines=50

cd dsd-fme

@REM log.ans should refer to your log output file after '2>' i.e. '2> log.ans'

tail.exe -n 40 -f log.ans
@REM Alternatively, you can also grep out keywords or interest, see example below
@REM tail.exe -f log.ans | grep "Keyword1\|Keyword2\|Keyword3"
@REM tail.exe -f log.ans | grep "Grant\|C+\|Keyword3"
@REM

@REM PAUSE
