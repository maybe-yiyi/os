.global context_switch
.type context_switch, @function

# context_switch(prev_context, next_context)
# prev_context in 4(%esp)
# next_context in 8(%esp)

context_switch:
    # Save previous context
    mov 4(%esp), %eax       # Get prev context pointer
    pushf
    push %ebp
    push %edi
    push %esi
    push %edx
    push %ecx
    push %ebx
    push %eax               # Save EAX
    mov %esp, (%eax)        # Save ESP to prev->context.esp
    
    # Load next context
    mov 8(%esp), %eax       # Get next context pointer (offset by pushed regs)
    mov (%eax), %esp        # Restore ESP from next->context.esp
    pop %ebx                # Restore EAX (was saved first, so pop last due to stack order)
    pop %ebx
    pop %ecx
    pop %edx
    pop %esi
    pop %edi
    pop %ebp
    popf
    
    ret