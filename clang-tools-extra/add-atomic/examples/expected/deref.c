void foo() {
  float f;
  float * _Atomic  target;
  target = &f;
  float t = *target;
  float * _Atomic * p = &target;
  float * q = *p;
  float * _Atomic ** r = &p;
  float * _Atomic * s = *r;
}
