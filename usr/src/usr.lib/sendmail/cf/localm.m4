############################################################
############################################################
#####
#####		Local and Program Mailer specification
#####
#####		@(#)localm.m4	3.5		2/24/83
#####
############################################################
############################################################

Mlocal,	P=/bin/mail, F=rlsDFMmn, S=10, R=20, A=mail -d $u
Mprog,	P=/bin/csh,  F=lsDFMe,   S=10, R=20, A=csh -fc $u

S10
R@			MAILER-DAEMON			errors to mailer-daemon
