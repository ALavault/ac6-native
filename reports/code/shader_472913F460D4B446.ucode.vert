/*    0.0 */       exec    // PredicateClean=false
/*    3   */          vfetch_full r0._xyw, r0.x, vf0, DataFormat=FMT_32_32_32_32_FLOAT, Stride=13, Signed=true, NumFormat=integer, PrefetchCount=8
/*    4   */          vfetch_mini r1.zwxy, Offset=4, DataFormat=FMT_32_32_32_32_FLOAT, Signed=true, NumFormat=integer
/*    5   */          vfetch_full r2.zyxw, r0.x, vf0, Offset=12, DataFormat=FMT_8_8_8_8, Stride=13
/*    6   */          serialize
                      setp_ne r0._, r0.w
/*    7   */     (p0) mad r0.x___, r0.wwww, c255.zzzz, c255.yyyy
/*    8   */     (p0) frcs r0.x___, r0.x
/*    0.1 */  (p0) exec
/*    9   */     (p0) mad r3.__z_, r0.xxxx, c255.wwww, c255.xxxx
/*   10   */     (p0) add r3.xy__, r0.yzzz, -r1.xyyy
              +  (p0) sin r0.x___, r3.z
/*   11   */     (p0) mul r0.___w, r3.yyyy, -r0.xxxx
              +  (p0) cos r0._y__, r3.z
/*   12   */     (p0) mul r0.xyz_, r3.xxyy, r0.xyyy
/*   13   */     (p0) add r0.xy__, r0.xyyy, r0.zwww
/*   14   */     (p0) add r0._yz_, r0.yyxx, r1.xxyy
/*    1.0 */       alloc position
/*    1.1 */       exec
/*   15   */          dp2add oPos.x___, r0.yzzz, c40.xyyy, c40.wwww
/*   16   */          dp2add oPos._y__, r0.yzzz, c41.xyyy, c41.wwww
/*   17   */          dp2add oPos.__z_, r0.yzzz, c42.xyyy, c42.wwww
/*   18   */          dp2add oPos.___w, r0.yzzz, c43.xyyy, c43.wwww
/*    2.0 */       alloc interpolators
/*    2.1 */       exece
/*   19   */          max o0.xy__, r1.zwww, r1.zwww
/*   20   */          max o1, r2, r2
