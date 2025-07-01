; Prompt the user for their name and print a greeting.

org 100h

start:
  mov dx, prompt_msg
  mov ah, 09h
  int 21h

  mov dx, input_buffer
  mov ah, 0Ah
  int 21h

  mov dx, newline
  mov ah, 09h
  int 21h

  mov dx, hello_msg
  mov ah, 09h
  int 21h

  mov al, [input_buffer + 1]
  mov bl, al
  xor bh, bh
  mov byte [input_buffer + 2 + bx], '$'

  mov dx, input_buffer + 2
  mov ah, 09h
  int 21h

  mov dx, exclamation
  mov ah, 09h
  int 21h

  mov ah, 4Ch
  mov al, 0
  int 21h

; Data
prompt_msg      db 'Please enter your name: $'
hello_msg       db 'Hello, $'
exclamation     db '!', 0Dh, 0Ah, '$'
newline         db 0Dh, 0Ah, '$'
input_buffer    db 50, 0
db 50 dup(0)
