void foo() {
  int target;
  int * a;
  a = ((void)1, &target);
  (void)a;
}
