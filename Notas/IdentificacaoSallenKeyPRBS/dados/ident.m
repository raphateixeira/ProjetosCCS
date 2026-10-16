clear, clc

m = 1
b = 2
k = 0.5

num = 1;
den = [m, b, k];

Gs = tf(num,den)
Ts = 0.1
Gz = c2d(Gs, Ts)