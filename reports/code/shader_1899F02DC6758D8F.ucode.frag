/*    0.0 */       exec
/*    2   */          tfetch2D r1.x___, r0.xy, tf0
/*    3   */          tfetch2D r1._x__, r0.xy, tf1
/*    4   */          tfetch2D r1.__x_, r0.xy, tf2
/*    0.1 */       alloc colors
/*    1.0 */       exece
/*    5   */          add r1.xyz_, r1.xyzz, c254.xyyy
/*    6   */          mul r0, r1.zzyx, c255
/*    7   */          add oC0.x0z0, r0.wwww, r0.yzzz
/*    8   */          mad r1.x___, -r1.yyyy, c254.zzzz, -r0.xxxx
/*    9   */          add oC0._y__, r1.xxxx, r0.wwww
/*    1.1 */       cnop
