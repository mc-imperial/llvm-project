void foo() {
  float f;
  float * target;
  target = &f;
  float t = *target;
  float ** p = &target;
  float * q = *p;
  float *** r = &p;
  float ** s = *r;
  (void)q;
  (void)s;
  (void)t;
}
