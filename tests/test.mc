int compiled_fn(int arg) {
    int x = arg;
    int y = x > 10 ? x * 2 : x + 5;

    if (y > 20) {
        return y;
    } else {
        return y - 1;
    }
}
