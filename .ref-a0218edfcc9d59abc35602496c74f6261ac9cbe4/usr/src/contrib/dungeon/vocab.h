C
C VOCABULARIES
C
	COMMON /BUZVOC/ BVOC(20)
	COMMON /PRPVOC/ PVOC(45)
	COMMON /DIRVOC/ DVOC(75)
	INTEGER AVOC(450)
	COMMON /ADJVOC/ AVOC1(184),AVOC2(114),AVOC3(106),AVOCND
	INTEGER VVOC(950)
	COMMON /VRBVOC/ VVOC1(92),VVOC1A(108),VVOC1B(38),VVOC2(104),
&		VVOC3(136),
&		VVOC4(116),VVOC5(134),VVOC6(117),VVOC7(89),VVOCND
	INTEGER OVOC(1050)
	COMMON /OBJVOC/ OVOC1(160),OVOC2(144),OVOC3(150),OVOC4(128),
&		OVOC5(111),OVOC6(104),OVOC6A(97),OVOC7(127),OVOCND
C
	EQUIVALENCE (VVOC(1),VVOC1(1))
	EQUIVALENCE (AVOC(1),AVOC1(1))
	EQUIVALENCE (OVOC(1),OVOC1(1))
