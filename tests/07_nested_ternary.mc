int compiled_fn(int arg) {
    int x = arg < 0 ? -1 : (arg == 0 ? 0 : 1);
    return x;
}
