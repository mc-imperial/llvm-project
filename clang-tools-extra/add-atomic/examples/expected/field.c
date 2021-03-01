struct S {
  int _Atomic  target;
};

void foo() {
  struct S myS;
  int _Atomic * foo = &myS.target;
}
