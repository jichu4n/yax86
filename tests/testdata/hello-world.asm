; hello-world.asm
; A simple hello world program for 8086 (COM format)
; To assemble with FASM: fasm hello-world.asm hello-world.com

org 100h        ; COM programs start at offset 100h

start:
    mov ah, 09h         ; DOS function: print string
    mov dx, hello_msg   ; DS:DX points to string
    int 21h             ; Call DOS

    mov ah, 4Ch         ; DOS function: terminate program
    mov al, 0           ; Return code 0
    int 21h             ; Call DOS

hello_msg db 'Hello, World!', 0Dh, 0Ah, '$'    ; Message to display
                                                ; 0Dh, 0Ah = CR, LF (new line)
                                                ; $ = string terminator for DOS