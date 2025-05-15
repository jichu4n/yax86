; 8086 assembly MOV instruction test cases in FASM syntax
; Register to register moves
mov ax, bx       ; 16-bit register to 16-bit register
mov cx, dx       ; Another 16-bit register to 16-bit register
mov al, bl       ; 8-bit register to 8-bit register
mov ch, dh       ; High 8-bit register to high 8-bit register
mov cl, dl       ; Low 8-bit register to low 8-bit register

; Immediate to register moves
mov ax, 0x1234   ; 16-bit immediate to 16-bit register
mov bx, 0x5678   ; Another 16-bit immediate to 16-bit register
mov dl, 0x42     ; 8-bit immediate to 8-bit register
mov ch, 0xFF     ; 8-bit immediate to high 8-bit register

; Memory to register moves
mov ax, [0x1000] ; 16-bit memory to 16-bit register (direct addressing)
mov bx, [si]     ; 16-bit memory to 16-bit register (indirect addressing)
mov cx, [bp+0x10]; 16-bit memory to 16-bit register (based addressing with displacement)
mov dx, [bx+si]  ; 16-bit memory to 16-bit register (indexed addressing)
mov al, [di+0x20]; 8-bit memory to 8-bit register (indexed addressing with displacement)
mov ah, [bx+di+5]; 8-bit memory to 8-bit register (based indexed addressing with displacement)

; Register to memory moves
mov [0x2000], ax ; 16-bit register to 16-bit memory (direct addressing)
mov [di], bx     ; 16-bit register to 16-bit memory (indirect addressing)
mov [bp+0x30], cx; 16-bit register to 16-bit memory (based addressing with displacement)
mov [bx+di], dx  ; 16-bit register to 16-bit memory (indexed addressing)
mov [si+0x40], al; 8-bit register to 8-bit memory (indexed addressing with displacement)
mov [bx+si+10], ah; 8-bit register to 8-bit memory (based indexed addressing with displacement)

; Immediate to memory moves
mov word [0x3000], 0xABCD ; 16-bit immediate to 16-bit memory (direct addressing)
mov byte [di+5], 0x42     ; 8-bit immediate to 8-bit memory (indexed addressing with displacement)
mov word [bx+si], 0x9876  ; 16-bit immediate to 16-bit memory (indexed addressing)

; Segment register moves
mov ds, ax       ; General register to segment register
mov es, bx       ; Another general register to segment register
mov cx, ss       ; Segment register to general register
mov dx, cs       ; Another segment register to general register

; Special cases
mov ax, [es:di]    ; Memory addressed with explicit segment override
mov bx, [cs:0x100] ; Direct addressing with segment override
mov cx, [ss:bp+10] ; Based addressing with segment override

; Edge cases
mov ax, 0        ; Zero immediate
mov bx, 0xFFFF   ; Maximum 16-bit immediate
mov cl, 0x7F     ; 8-bit positive immediate
mov ch, 0x80     ; 8-bit negative immediate (signed interpretation)
