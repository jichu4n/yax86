; This program asks for the user's birth month and day,
; then calculates and displays their astrological sign.
; To be compiled as a .COM file.
;
; Corrected version: Fixes the input handling routine to prevent buffer errors.

    org  100h

start:
    ; Display the prompt for the month
    mov  dx, month_prompt
    mov  ah, 9
    int  21h

    ; Get the month input from the user
    call get_two_digit_number
    mov  [month], al

    ; Display the prompt for the day
    mov  dx, day_prompt
    mov  ah, 9
    int  21h

    ; Get the day input from the user
    call get_two_digit_number
    mov  [day], al

    ; Now, determine the sign
    call calculate_sign

    ; Print the result
    mov  dx, result_msg
    mov  ah, 9
    int  21h

    ; Display the sign name
    mov  dx, [sign_ptr]
    mov  ah, 9
    int  21h

    ; Newline for clean exit
    mov  dx, crlf
    mov  ah, 9
    int  21h

    ; Exit to DOS
    mov  ax, 4C00h
    int  21h

;-------------------------------------------------------------
; get_two_digit_number: Reads a two-digit number from the user
;                       using the robust DOS buffered input function.
; Output: AL = the number
;-------------------------------------------------------------
get_two_digit_number:
    ; Set up buffer for DOS function 0Ah (buffered input)
    mov  dx, input_buffer
    mov  ah, 0Ah
    int  21h

    ; Print a newline, as function 0Ah does not echo the Enter key press
    mov  dx, crlf
    mov  ah, 9
    int  21h

    ; Convert the two ASCII digits in the buffer to a number.
    ; The actual string starts at the third byte of the buffer.
    ; First digit is at input_buffer + 2
    mov  al, [input_buffer + 2]
    sub  al, '0'
    mov  bl, 10
    mul  bl
    mov  [temp_digit], al

    ; Second digit is at input_buffer + 3
    mov  al, [input_buffer + 3]
    sub  al, '0'
    add  al, [temp_digit]

    ret

;-------------------------------------------------------------
; calculate_sign: Determines the horoscope sign based on month and day.
; Input: [month] and [day] variables
; Output: [sign_ptr] points to the correct sign string
;-------------------------------------------------------------
calculate_sign:
    mov  si, signs_data
    mov  cx, 12

check_next_sign:
    mov  al, [month]
    cmp  al, [si]      ; Compare user month with table month
    ja   found_sign    ; If user month > table month, we found the sign.
    jb   no_match      ; If user month < table month, it's not this sign.

    ; If we get here, months are equal. Check the day.
    mov  al, [day]
    cmp  al, [si+2]    ; Compare user day with table day
    jae  found_sign    ; If user day >= table day, we found the sign.

    ; Otherwise, the date is before the start of this sign.
    jmp  no_match

found_sign:
    mov  bx, [si+3]
    mov  [sign_ptr], bx
    ret

no_match:
    add  si, 5
    loop check_next_sign

    ; If the loop completes, no match was found. This means the date
    ; is in early January, which belongs to Capricorn.
    mov  bx, capricorn
    mov  [sign_ptr], bx
    ret

;-------------------------------------------------------------
; Data definitions
;-------------------------------------------------------------

    month_prompt db 'Enter your birth month (MM): $'
    day_prompt   db 0Dh, 0Ah, 'Enter your birth day (DD):   $'
    result_msg   db 0Dh, 0Ah, 'Your astrological sign is: $'
    crlf         db 0Dh, 0Ah, '$'

    month        db 0
    day          db 0
    temp_digit   db 0
    sign_ptr     dw 0

    ; Buffer for reading user input
input_buffer:
    db 4     ; Max chars to read (2 digits + CR + room for error)
    db 0     ; Actual chars read (filled by DOS)
    db 5 dup(0) ; Buffer for the string itself

    ; Zodiac Sign Data Table
    ; Format: Start Month (byte), padding (byte), Start Day (byte), Pointer to Name (word)
    ; The signs are ordered by their start date throughout the year in reverse.
    signs_data:
        db 12, 0, 22
        dw capricorn
        db 11, 0, 22
        dw sagittarius
        db 10, 0, 23
        dw scorpio
        db  9, 0, 23
        dw libra
        db  8, 0, 23
        dw virgo
        db  7, 0, 23
        dw leo
        db  6, 0, 21
        dw cancer
        db  5, 0, 21
        dw gemini
        db  4, 0, 20
        dw taurus
        db  3, 0, 21
        dw aries
        db  2, 0, 19
        dw pisces
        db  1, 0, 20
        dw aquarius

    ; Sign Name Strings
    capricorn   db 'Capricorn$'
    aquarius    db 'Aquarius$'
    pisces      db 'Pisces$'
    aries       db 'Aries$'
    taurus      db 'Taurus$'
    gemini      db 'Gemini$'
    cancer      db 'Cancer$'
    leo         db 'Leo$'
    virgo       db 'Virgo$'
    libra       db 'Libra$'
    scorpio     db 'Scorpio$'
    sagittarius db 'Sagittarius$'
