01/01/1970 10:00:00.000000 anon0 @-1:-1 L 1.1.1.1,1025 -> 2.2.2.2,25 PR tcp len 20 40 -S IN
01/01/1970 10:00:00.000000 anon0 @-1:-1 L 1.1.1.1,1025 -> 2.2.2.2,25 PR tcp len 20 40 -A IN
01/01/1970 10:00:00.000000 anon0 @-1:-1 L 2.2.2.2,25 -> 1.1.1.1,1025 PR tcp len 20 40 -AS IN
01/01/1970 10:00:00.000000 anon0 @-1:-1 L 1.1.1.1,1025 -> 2.2.2.2,25 PR tcp len 20 40 -F IN
01/01/1970 10:00:00.000000 2x anon0 @-1:-1 L 1.1.1.1,1025 -> 2.2.2.2,25 PR tcp len 20 40 -A IN
01/01/1970 10:00:00.000000 anon0 @-1:-1 L 1.1.1.1,1 -> 4.4.4.4,53 PR udp len 20 40 IN
01/01/1970 10:00:00.000000 2x anon0 @-1:-1 L 2.2.2.2,1 -> 4.4.4.4,53 PR udp len 20 40 IN
01/01/1970 10:00:00.000000 anon0 @-1:-1 L 2.2.2.2 -> 4.4.4.4 PR ip len 20 (20) IN
01/01/1970 10:00:00.000000 anon0 @-1:-1 L 3.3.3.3,1023 -> 1.1.1.1,2049 PR udp len 20 28 IN
01/01/1970 10:00:00.000000 anon0 @-1:-1 L 1.1.1.1,2049 -> 3.3.3.3,1023 PR udp len 20 28 IN
--------
--------
--------
01/01/1970 10:00:00.000000 anon0 @0:1 p 2.2.2.2,25 -> 1.1.1.1,1025 PR tcp len 20 40 -AS IN
01/01/1970 10:00:00.000000 2x anon0 @0:1 p 2.2.2.2,1 -> 4.4.4.4,53 PR udp len 20 40 IN
01 02 03 04 05 06 07 08 09 0a 0b 0d                    ............
01/01/1970 10:00:00.000000 anon0 @0:1 p 2.2.2.2 -> 4.4.4.4 PR ip len 20 (20) IN
--------
01/01/1970 10:00:00.000000 anon0 @0:1 p 1.1.1.1,1025 -> 2.2.2.2,25 PR tcp len 20 40 -S K-S IN
01/01/1970 10:00:00.000000 anon0 @0:1 p 1.1.1.1,1025 -> 2.2.2.2,25 PR tcp len 20 40 -A K-S IN
01/01/1970 10:00:00.000000 anon0 @0:1 p 2.2.2.2,25 -> 1.1.1.1,1025 PR tcp len 20 40 -AS K-S IN
01/01/1970 10:00:00.000000 e1 @0:1 p 2.2.2.2,25 -> 1.1.1.1,1025 PR tcp len 20 40 -A K-S OUT
01/01/1970 10:00:00.000000 anon0 @0:1 p 1.1.1.1,1025 -> 2.2.2.2,25 PR tcp len 20 40 -F K-S IN
--------
01/01/1970 10:00:00.000000 anon0 @0:1 p 1.1.1.1,1025 -> 2.2.2.2,25 PR tcp len 20 40 -S K-S IN
--------
01/01/1970 10:00:00.000000 anon0 @-1:-1 L 1.1.1.1,1025 -> 2.2.2.2,25 PR tcp len 20 40 -S IN
01/01/1970 10:00:00.000000 anon0 @0:4 p 1.1.1.1,1025 -> 2.2.2.2,25 PR tcp len 20 40 -S K-S IN
01/01/1970 10:00:00.000000 anon0 @0:4 p 1.1.1.1,1025 -> 2.2.2.2,25 PR tcp len 20 40 -A K-S IN
01/01/1970 10:00:00.000000 anon0 @0:4 p 2.2.2.2,25 -> 1.1.1.1,1025 PR tcp len 20 40 -AS K-S IN
01/01/1970 10:00:00.000000 e1 @0:4 p 2.2.2.2,25 -> 1.1.1.1,1025 PR tcp len 20 40 -A K-S OUT
01/01/1970 10:00:00.000000 anon0 @0:4 p 1.1.1.1,1025 -> 2.2.2.2,25 PR tcp len 20 40 -F K-S IN
01/01/1970 10:00:00.000000 2x anon0 @-1:-1 L 1.1.1.1,1025 -> 2.2.2.2,25 PR tcp len 20 40 -A IN
01/01/1970 10:00:00.000000 anon0 @-1:-1 L 1.1.1.1,1 -> 4.4.4.4,53 PR udp len 20 40 IN
01/01/1970 10:00:00.000000 anon0 @-1:-1 L 2.2.2.2,1 -> 4.4.4.4,53 PR udp len 20 40 IN
01/01/1970 10:00:00.000000 anon0 @0:3 p 2.2.2.2,1 -> 4.4.4.4,53 PR udp len 20 40 IN
01 02 03 04 05 06 07 08 09 0a 0b 0d                    ............
01/01/1970 10:00:00.000000 anon0 @-1:-1 L 2.2.2.2,1 -> 4.4.4.4,53 PR udp len 20 56 IN
01/01/1970 10:00:00.000000 anon0 @0:3 p 2.2.2.2,1 -> 4.4.4.4,53 PR udp len 20 56 IN
01 02 03 04 05 06 07 08 09 0a 0b 0d 0e 0f 40 61        ..............@a
42 63 44 65 46 67 48 69 4a 6b 4c 6d                    BcDeFgHiJkLm
01/01/1970 10:00:00.000000 anon0 @-1:-1 L 2.2.2.2 -> 4.4.4.4 PR ip len 20 (20) IN
01/01/1970 10:00:00.000000 anon0 @0:3 p 2.2.2.2 -> 4.4.4.4 PR ip len 20 (20) IN
01/01/1970 10:00:00.000000 anon0 @-1:-1 L 3.3.3.3,1023 -> 1.1.1.1,2049 PR udp len 20 28 IN
01/01/1970 10:00:00.000000 anon0 @100:1 p 3.3.3.3,1023 -> 1.1.1.1,2049 PR udp len 20 28 IN
01/01/1970 10:00:00.000000 anon0 @-1:-1 L 1.1.1.1,2049 -> 3.3.3.3,1023 PR udp len 20 28 IN
--------
