#!/usr/local/bin/perl

&doit(100,"Pentium   100 32",0.0195,0.1000,0.6406,4.6100);	# pentium-100
&doit(200,"PPro      200 32",0.0070,0.0340,0.2087,1.4700);	# pentium-100
&doit( 25,"R3000      25 32",0.0860,0.4825,3.2417,23.8833);	# R3000-25
&doit(200,"R4400     200 32",0.0137,0.0717,0.4730,3.4367);	# R4400 32bit
&doit(180,"R10000    180 32",0.0061,0.0311,0.1955,1.3871);	# R10000 32bit
&doit(180,"R10000    180 64",0.0034,0.0149,0.0880,0.5933);	# R10000 64bit
&doit(400,"DEC 21164 400 64",0.0022,0.0105,0.0637,0.4457);	# R10000 64bit

sub doit
	{
	local($mhz,$label,@data)=@_;

	for ($i=0; $i <= $#data; $i++)
		{
		$data[$i]=1/$data[$i]*200/$mhz;
		}
	printf("%s %6.1f %6.1f %6.1f %6.1f\n",$label,@data);
	}

