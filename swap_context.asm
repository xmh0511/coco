.model flat, c
;public swap_context
.code
swap_context proc
mov eax,[esp+4]
mov [eax+16],ecx
xor ecx,ecx
lea ecx,[esp+4]
mov [eax+4],ecx
xor ecx,ecx
mov ecx,[esp]
mov [eax],ecx
mov [eax+8],ebp
mov [eax+12],ebx
mov [eax+20],edx
mov [eax+24],edi
mov [eax+28],esi
xor eax,eax
mov eax,[esp+8]
mov esp,[eax+4]
mov ebp,[eax+8]
mov ebx,[eax+12]
mov ecx,[eax+16]
mov edx,[eax+20]
mov edi,[eax+24]
mov esi,[eax+28]
mov eax,[eax]
jmp eax
swap_context endp
end
