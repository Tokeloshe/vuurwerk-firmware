// Minimal syscalls stub - we don't use heap but library might reference it
void *_sbrk(int incr) {
    (void)incr;
    return (void *)-1; // Always fail - no heap allocation allowed
}
