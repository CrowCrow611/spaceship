; counter.asm
; Counts from 1 to 10, printing each number to the terminal.
; Demonstrates: MOVI, ADDI, CMPI, JLE, STORE, HLT
 
    MOVI  R10, 0x1E000011   ; R10 = address of print-int port
    MOVI  R6,  1            ; R6  = counter, starts at 1
    MOVI  R7,  10           ; R7  = loop limit
 
loop:
    STORE [R10], R6         ; print current counter value
    ADDI  R6, R6, 1         ; counter++
    CMPI  R6, 11            ; compare counter to 11
    JL    loop              ; if counter < 11, loop again
 
    HLT                     ; done
 