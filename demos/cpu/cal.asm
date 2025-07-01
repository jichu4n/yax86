; 8086 Assembly Calendar Program for FASM
; Generates a calendar for a given month and year.
; Monday is the first day of the week.
; Corrected for .COM file format.

    org 100h

start:
    ; --- Get Year Input ---
    mov dx, year_prompt
    mov ah, 9
    int 21h

    mov dx, input_buffer
    mov ah, 0Ah
    int 21h
    call print_newline

    ; Convert year string to number
    mov si, input_buffer + 2
    call str_to_int
    mov [year], cx

    ; --- Get Month Input ---
    mov dx, month_prompt
    mov ah, 9
    int 21h

    mov dx, input_buffer
    mov ah, 0Ah
    int 21h
    call print_newline

    ; Convert month string to number
    mov si, input_buffer + 2
    call str_to_int
    mov [month], cx

    ; --- Calendar Logic ---
    call calculate_start_day
    call print_calendar

    ; --- Exit Program ---
    mov ax, 4C00h
    int 21h


; ===============================================
;              PROCEDURES
; ===============================================

; -----------------------------------------------
; Procedure: str_to_int
; Converts an ASCII string of digits to a 16-bit integer.
; Input: SI -> address of the string
; Output: CX -> converted integer
; -----------------------------------------------
str_to_int:
    xor cx, cx      ; Clear result (CX)
    xor bx, bx      ; Clear temp register
.loop:
    mov bl, [si]    ; Get character
    inc si
    cmp bl, 13      ; Check for carriage return (end of input)
    je .done
    cmp bl, 10      ; Check for carriage return (end of input)
    je .done
    cmp bl, '0'
    jb .loop        ; Skip if not a digit
    cmp bl, '9'
    ja .loop        ; Skip if not a digit

    sub bl, '0'     ; Convert ASCII char to digit
    mov ax, cx
    mov dx, 10
    mul dx          ; Multiply current result by 10
    add ax, bx      ; Add new digit
    mov cx, ax
    jmp .loop
.done:
    ret

; -----------------------------------------------
; Procedure: print_newline
; Prints a carriage return and line feed.
; -----------------------------------------------
print_newline:
    mov dx, newline
    mov ah, 9
    int 21h
    ret

; -----------------------------------------------
; Procedure: calculate_start_day
; Calculates the day of the week for the 1st of the month using a variation of Zeller's congruence.
; Monday = 0, Tuesday = 1, ..., Sunday = 6
; Output: [start_day] -> The starting day of the week.
; -----------------------------------------------
calculate_start_day:
    mov ax, [month]
    mov cx, [year]

    ; Adjust month and year for algorithm (Jan/Feb are months 13/14 of previous year)
    cmp ax, 3
    jge .no_adjust
    dec cx
    add ax, 12
.no_adjust:
    mov [temp_year], cx
    mov [temp_month], ax

    ; Zeller's Congruence: h = (q + floor(13*(m+1)/5) + K + floor(K/4) + floor(J/4) - 2*J) mod 7
    ; where q=1 (day), m=month, K=year%100, J=floor(year/100)
    ; Our formula is simplified and adjusted for Monday=0.

    mov ax, [temp_year]
    xor dx, dx
    mov bx, 100
    div bx          ; AX = J (century), DX = K (year of century)

    mov [J], ax
    mov [K], dx

    ; Term 1: floor(13 * (m + 1) / 5)
    mov ax, [temp_month]
    inc ax
    mov bx, 13
    mul bx
    mov bx, 5
    div bx          ; AX = floor(13*(m+1)/5)
    mov si, ax

    ; Term 2: K
    add si, [K]

    ; Term 3: floor(K / 4)
    mov ax, [K]
    xor dx, dx
    mov bx, 4
    div bx
    add si, ax

    ; Term 4: floor(J / 4)
    mov ax, [J]
    xor dx, dx
    mov bx, 4
    div bx
    add si, ax

    ; Term 5: 5 * J (simplified from -2*J and adjusted for different base)
    mov ax, [J]
    mov bx, 5
    mul bx
    add si, ax

    ; Term 6: q (day = 1)
    inc si

    ; Final modulo 7
    mov ax, si
    xor dx, dx
    mov bx, 7
    div bx          ; DX = h (day of week, 0=Sat, 1=Sun, ..., 6=Fri)

    ; Adjust from Sat=0 to Mon=0
    ; Sat(0)->5, Sun(1)->6, Mon(2)->0, Tue(3)->1, Wed(4)->2, Thu(5)->3, Fri(6)->4
    cmp dx, 2
    jge .adjust
    add dx, 5
    jmp .store
.adjust:
    sub dx, 2
.store:
    mov [start_day], dl
    ret


; -----------------------------------------------
; Procedure: print_calendar
; Prints the formatted calendar for the month.
; -----------------------------------------------
print_calendar:
    ; Print Header
    mov dx, header
    mov ah, 9
    int 21h
    call print_newline

    ; Determine number of days in the month
    mov cx, [year]
    mov ax, [month]
    cmp ax, 2
    je .february

    ; Months with 30 days: 4, 6, 9, 11
    cmp ax, 4
    je .days_30
    cmp ax, 6
    je .days_30
    cmp ax, 9
    je .days_30
    cmp ax, 11
    je .days_30
    jmp .days_31 ; All others have 31

.february:
    ; Leap year check: divisible by 4, unless divisible by 100 but not by 400
    mov ax, cx
    xor dx, dx
    mov bx, 4
    div bx
    cmp dx, 0
    jne .days_28 ; Not divisible by 4

    mov ax, cx
    xor dx, dx
    mov bx, 100
    div bx
    cmp dx, 0
    jne .days_29 ; Divisible by 4, not by 100

    mov ax, cx
    xor dx, dx
    mov bx, 400
    div bx
    cmp dx, 0
    jne .days_28 ; Divisible by 100, not by 400

.days_29:
    mov word [days_in_month], 29
    jmp .print_days
.days_28:
    mov word [days_in_month], 28
    jmp .print_days
.days_30:
    mov word [days_in_month], 30
    jmp .print_days
.days_31:
    mov word [days_in_month], 31
    ; jmp .print_days ; Fall through

.print_days:
    ; Print leading spaces
    mov cl, [start_day]
    mov ch, 0
    cmp cl, 0
    je .day_loop_start
.space_loop:
    ; Print 3 spaces for alignment
    mov dl, ' '
    mov ah, 2
    int 21h
    int 21h
    int 21h
    loop .space_loop

.day_loop_start:
    mov cx, 1               ; Day counter
    mov bl, [start_day]     ; Day of week counter

.day_loop:
    ; Print the day number
    mov ax, cx
    call print_number

    ; Increment day of week counter
    inc bl
    cmp bl, 7
    jne .no_newline
    ; If end of week (Sunday), print newline
    mov bl, 0
    call print_newline
.no_newline:
    ; Check if we've printed all days
    inc cx
    cmp cx, [days_in_month]
    jle .day_loop

    call print_newline
    ret


; -----------------------------------------------
; Procedure: print_number (FIXED)
; Prints a 16-bit number, ensuring proper padding for calendar alignment.
; Preserves the BX register.
; Input: AX -> number to print
; -----------------------------------------------
print_number:
    push bx         ; Save BX because we use it for division
    cmp ax, 10
    jl .single_digit

; --- Two-digit case ---
    xor dx, dx
    mov bx, 10
    div bx          ; ax = quotient, dx = remainder
    add al, '0'     ; Convert first digit to ASCII
    add dl, '0'     ; Convert second digit to ASCII

    push dx         ; Save second digit on stack
    mov dl, al      ; Move first digit into dl for printing
    mov ah, 2
    int 21h         ; Print first digit
    pop dx          ; Restore second digit into dl
    mov ah, 2
    int 21h         ; Print second digit
    jmp .add_space

.single_digit:
    ; Print leading space for alignment
    mov dl, ' '
    mov ah, 2
    int 21h
    ; Print the digit
    add al, '0'     ; Convert digit to ASCII
    mov dl, al
    mov ah, 2
    int 21h

.add_space:
    ; Print trailing space for alignment
    mov dl, ' '
    mov ah, 2
    int 21h
    pop bx          ; Restore BX before returning
    ret

; ===============================================
;                 DATA
; ===============================================

; --- Prompts and Strings ---
year_prompt     db 'Enter year (e.g., 2024): $'
month_prompt    db 'Enter month (1-12): $'
header          db 'Mo Tu We Th Fr Sa Su'
newline         db 13, 10, '$'

; --- Variables ---
; Buffer for INT 21h/0Ah. Sized for max 6 chars + terminating CR.
input_buffer    db 6, 0, '       '
year            dw 0
month           dw 0
temp_year       dw 0
temp_month      dw 0
days_in_month   dw 31
start_day       db 0        ; 0=Mon, 1=Tue, ..., 6=Sun
J               dw 0        ; Century
K               dw 0        ; Year of the century
