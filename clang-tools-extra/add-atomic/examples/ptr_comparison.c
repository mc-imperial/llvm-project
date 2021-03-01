void foo() {
  int target;
  int* p = &target;
  int* q = 0;
  int b = p == q;
  (void)b;
}
