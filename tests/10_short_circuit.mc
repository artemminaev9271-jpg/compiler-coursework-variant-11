int compiled_fn(int arg) {
    if (arg < 0 && 10 / 0 > 0) {
        return 1;
    } else {
        return 123;
    }
}
