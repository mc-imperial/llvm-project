struct S {
  int _Atomic * f;
};

void foo() {
  int _Atomic  target;
  struct S myS = {&target};
  (void)myS;
}
