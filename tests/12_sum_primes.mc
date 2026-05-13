int is_prime(int n) {
    if (n < 2) {
        return 0;
    } else {
        int ok = 1;
        for (int d = 2; d * d <= n; d = d + 1) {
            if (n % d == 0) {
                ok = 0;
            }
        }
        return ok;
    }
}

int compiled_fn(int arg) {
    int sum = 0;
    for (int x = 2; x <= arg; x = x + 1) {
        if (is_prime(x) == 1) {
            sum = sum + x;
        }
    }
    return sum;
}
