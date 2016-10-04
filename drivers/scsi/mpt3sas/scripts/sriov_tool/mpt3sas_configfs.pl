#!/usr/bin/perl
#
#      usage:
#            - When mpt3sas_configfs.pl is run for the first time, issue the
#              following command
#              ./mpt3sas_configfs.pl discover
#
#            - When the system gets booted, issue the following command, so
#              that earlier settings are preserved
#              ./mpt3sas_configfs.pl start
#
#            - Before unloading the driver, issue the following command, so
#              that files under /config are removed
#              ./mpt3sas_configfs.pl stop
#
#            - When the changes had to be made to the configfs then issue the
#              command
#              ./mpt3sas_configfs.pl
#
#      Description: This application could create the needed filestructure for
#                   mpt3sas configfs and then based on user input could update
#                   the attributes.

use strict;
use warnings;

use XML::Simple;
use File::Find;

our %mpt3sas;
our @hbas;
our @attrs;
our @encls;
our $hash_modified = 0;
our $no_of_hba;
our $attr_max;
our $encl_max;

sub Create_XML_File
{
    my $xs = new XML::Simple(NoAttr=>1, RootName=>'mpt3sas');
    my $xml = $xs->XMLout(\%mpt3sas);

    open (XMLFILE, '>config.xml');
    print XMLFILE $xml;
    close (XMLFILE);
}

sub Read_XML_File
{
    my $xs = new XML::Simple();
    my $data = $xs->XMLin("config.xml") || die;

    %mpt3sas = %$data;
}

sub Write_Internally_Modified_Attributes_to_XML
{
    my $attr_val;
    
	foreach my $hba ( sort keys %mpt3sas ) {
		if( $hba =~ "hba") {
			for my $attr ( sort keys %{ $mpt3sas{$hba} } ) {
				if(($attr !~ "ioc_number") && ($attr !~ "mapping_mode") &&
				   ($attr !~ "description")) {
					for my $encl ( sort keys %{ $mpt3sas{$hba}{$attr}} ) {
						if ( $encl =~ "serial_number" ) {
							if(-e "/config/mpt3sas/$hba/$attr/serial_number") {
							open (ATTR_FH, "/config/mpt3sas/$hba/$attr/serial_number");
							$attr_val = <ATTR_FH>;
							chomp ($attr_val);
							if($mpt3sas{$hba}{$attr}{serial_number} ne $attr_val) {
								$mpt3sas{$hba}{$attr}{serial_number} = $attr_val;
								$hash_modified = 1;
							}
							close (ATTR_FH);	
							}
						}
						if ( $encl =~ "WWID" ) {
							if(-e "/config/mpt3sas/$hba/$attr/WWID") {
							open (ATTR_FH, "/config/mpt3sas/$hba/$attr/WWID");
							$attr_val = <ATTR_FH>;
							chomp ($attr_val);
							if($mpt3sas{$hba}{$attr}{WWID} ne $attr_val) {
								$mpt3sas{$hba}{$attr}{WWID} = $attr_val;
								$hash_modified = 1;
							}
							close (ATTR_FH);	
							}
						}
					}
				}
			}
		}
	}
}

sub Exit_Program
{
    Write_Internally_Modified_Attributes_to_XML;
    if($hash_modified == 1)
    {
	Create_XML_File
    }
    exit;
}

sub Start_Discovery
{
    my $dh;
    my @hosts;
    my @attr_files;
    my $proc_name;
    my $unique_id;
    my $attr_value;
    my @directory;

    if (not -d "/config/mpt3sas")
    {
	print "Configfs not mounted on /config\n";
	Exit_Program;
    }

    opendir($dh, "/sys/class/scsi_host") || die;
    @hosts = readdir $dh;
    closedir $dh;

    foreach my $host (@hosts)
    {
	if($host =~ "host")
	{
	    if (-e "/sys/class/scsi_host/$host/proc_name")
	    {
		open (PROC_NAME, "/sys/class/scsi_host/$host/proc_name");
		$proc_name = <PROC_NAME>;
		chomp ($proc_name);
		close (PROC_NAME);
		if ($proc_name =~ "mpt3sas")
		{
		    if (-e "/sys/class/scsi_host/$host/unique_id")
		    {
			open (UNIQUE_ID, "/sys/class/scsi_host/$host/unique_id");
			$unique_id = <UNIQUE_ID>;
			chomp ($unique_id);
			close (UNIQUE_ID);
		    }
		    else {
			print "An SAS Gen3 HBA exists but unique_id file is missing\n";
			Exit_Program;
		    }
		    my $hba_dir = "/config/mpt3sas/hba".$unique_id;
		    `mkdir $hba_dir`;
		}
	    } else {
		print "Some SCSI_HOST exists but proc_name file is missing\n";
		Exit_Program;
	    }
	}
    }
    push (@directory, "/config/mpt3sas");
    find(\&Slurp_Directory, @directory);
}

sub Slurp_Directory
{
    my $file = $_;
    my $content;
    my @field = split (/\//,$File::Find::name);

    if($#field == 3)
    {
	if( -d $File::Find::name)
	{
	    our $rec = {};
	    $mpt3sas{$field[3]} = $rec;
	}
	else
	{
	    open (READ_FILE, $File::Find::name);
	    $content = <READ_FILE>;
	    chomp ($content);
	    close (READ_FILE);
	    $mpt3sas{$field[3]} = $content;
	}
    }
    elsif ($#field == 4 )
    {
	if( -d $File::Find::name)
	{
	    our $rec1 = {};
	    $mpt3sas{$field[3]}{$field[4]} = $rec1;
	}
	else
	{
	    open (READ_FILE, $File::Find::name);
	    $content = <READ_FILE>;
	    chomp ($content);
	    close (READ_FILE);

	    $mpt3sas{$field[3]}{$field[4]} = $content;
	}
    }
    elsif ($#field == 5)
    {
	if( -d $File::Find::name)
	{
	    print "No directory should be there at this level\n";
	}
	else
	{
	    open (READ_FILE, $File::Find::name);
	    $content = <READ_FILE>;
	    chomp ($content);
	    close (READ_FILE);

	    $mpt3sas{$field[3]}{$field[4]}{$field[5]} = $content;
	}
    }
}

sub Recreate_files_from_XML_config
{
    my $file_val;
    my $ret;

#    `mount -t configfs none /config`;
    Read_XML_File;
    if (not -d "/config/mpt3sas")
    {
	print "Configfs not mounted on /config or mpt3sas driver not loaded\n";
	Exit_Program;
    }
    foreach my $hba ( sort keys %mpt3sas ) {
	if( $hba =~ "hba") {
	    my $hba_dir = "/config/mpt3sas/$hba";
	    `mkdir $hba_dir`;

# First write ioc_number, mapping_mode. We can't create enclosure directories
# right now as ioc_number & mapping_mode had to be set first.

	    foreach my $encl ( sort keys %{ $mpt3sas{$hba} } ) {
		if(($encl =~ "ioc_number") || ($encl =~ "mapping_mode"))
		{
		    if ((-e "/config/mpt3sas/$hba/$encl") && ($encl !~ "description"))
		    {
			$file_val = $mpt3sas{$hba}{$encl};
			`echo $file_val > /config/mpt3sas/$hba/$encl`;
			$ret = `echo $?`;
			if ($ret != 0)
			{
				print "Error while creating directory $hba $encl \n";
				Exit_Program;
			}
		    }
		}
	    }

# Create the enclosure directories now

	    foreach my $encl ( sort keys %{ $mpt3sas{$hba} } ) {
		if(($encl !~ "ioc_number") && ($encl !~ "mapping_mode") &&
						($encl !~ "description"))
		{
		my $encl_dir = "/config/mpt3sas/$hba/$encl";
		`mkdir $encl_dir`;
		foreach my $val ( sort keys %{ $mpt3sas{$hba}{$encl} } ) {
			if ((-e "/config/mpt3sas/$hba/$encl/$val") &&
				($val !~ "description"))
			{
				$file_val = $mpt3sas{$hba}{$encl}{$val};
				if($val =~ "WWID") {
					$file_val =~ s/[\(\)]//g;
				}
				if((($mpt3sas{$hba}{mapping_mode} == 1) && ($val !~ "serial_number")) ||
				  (($mpt3sas{$hba}{mapping_mode} == 2) && ($val !~ "slot"))) {
					if ($val =~ "serial_number") {
						`echo "$file_val" > /config/mpt3sas/$hba/$encl/$val`;
					} else {
						`echo $file_val > /config/mpt3sas/$hba/$encl/$val`;
					}
					$ret = `echo $?`;
					if ($ret != 0) {
					    print "Error while writing $val\n";
					    Exit_Program;
					}
				}
			}
		    }
		}
	    }
	}
	else {
	if ((-e "/config/mpt3sas/$hba") && ($hba !~ "description"))
	{
		$file_val = $mpt3sas{$hba};
		`echo $file_val > /config/mpt3sas/$hba`;
		$ret = `echo $?`;
		if ($ret != 0)
		{
		    print "Error while writing $hba\n";
		    Exit_Program;
		}
	    }
	}
    }
}

sub Cleanup_Mapping
{
    foreach my $hba ( sort keys %mpt3sas ) {
	if( $hba =~ "hba") {
	    for my $val ( sort keys %{ $mpt3sas{$hba} } ) {
		if(($val !~ "ioc_number") && ($val !~ "mapping_mode") &&
		   ($val !~ "description")) {
		    `rmdir /config/mpt3sas/$hba/$val`;
		}
	    }
	    `rmdir /config/mpt3sas/$hba`;
	}
    }
}

sub Print_Hash
{
    print "|\n";
    print "--mpt3sas\n";

    foreach my $hba ( sort keys %mpt3sas ) {
	if( $hba =~ "hba") {
	    print "  |--$hba\n";
	    for my $val ( sort keys %{ $mpt3sas{$hba} } ) {
		print "  |  |--$val = $mpt3sas{$hba}{$val}\n";
	    }
	}
	else {
	    print "  |--$hba = $mpt3sas{$hba}\n";
	}
	print "  |\n";
    }
}

sub Print_Hash1
{
    print "|\n";
    print "+--mpt3sas\n";

    foreach my $hba ( sort keys %mpt3sas ) {
	if( $hba =~ "hba") {
	    print "  +--$hba\n";
	    foreach my $encl ( sort keys %{ $mpt3sas{$hba} } ) {
	    if($encl =~ "Enclosure")
	    {
		    print "  |  +--$encl\n";
		    foreach my $val ( sort keys %{ $mpt3sas{$hba}{$encl}} ) {
			print "  |  |  \\--$val\n";
		    }
		}
		else
		{
		    print "  |  |--$encl\n";
		}
	    }
	}
	else {
	    print "  |--$hba\n";
	}
	print "  |\n";
    }
}

sub Push_HBAs_In_Array
{
    foreach my $hba ( sort keys %mpt3sas ) {
	if( $hba =~ "hba") {
	    push (@hbas, $hba);
	}
    }
    $no_of_hba = scalar(@hbas);
}

sub Update_HASH_attr
{
    my ($curr_hba,$attr,$attr_val) = @_;
    $mpt3sas{$hbas[$curr_hba]}{$attrs[$attr]} = $attr_val;
    $hash_modified = 1;
}

sub Update_HASH_encl
{
    my ($curr_hba,$attr,$encl,$attr_val) = @_;
    $mpt3sas{$hbas[$curr_hba]}{$attrs[$attr]}{$encls[$encl]} = $attr_val;
    $hash_modified = 1;
}

sub List_Attr
{
    my ($curr_hba) = @_;
    my $num = 1;
    my $option;
    @attrs = ();
    print "\n";
    print "\tAttribute Name\n";
    for my $val ( sort keys %{ $mpt3sas{$hbas[$curr_hba]} } ) {
	push (@attrs, $val);
	print " $num.\t$val\n";
	$num++;
    }

    $num--;
    $attr_max = $num;
    print "\n";
    print "Select an Attribute:  [1-$num or 0 to quit] ";
    $option = <>;
    chomp ($option);
    if($option !~ m/\d/)
    {
	print "Invalid Option Selected.\n";
	$option = 0;
    }
    return $option-1;
}

sub List_Encl
{
    my ($curr_hba, $curr_encl, $display) = @_;
    my $num = 1;
    my $allowed_num = 1;
    my $option;
    my $mapping_mode;
    my @allowed_nums;

    if(-e "/config/mpt3sas/$hbas[$curr_hba]/mapping_mode") {
	open (ATTR_FH, "/config/mpt3sas/$hbas[$curr_hba]/mapping_mode");
        $mapping_mode = <ATTR_FH>;
        chomp ($mapping_mode);
    } else {
	Exit_Program; 
    } 

    @encls = ();
    @allowed_nums = ();
    print "\n";
    print "\tAttribute Name\n";
    for my $val ( sort keys %{ $mpt3sas{$hbas[$curr_hba]}{$attrs[$curr_encl]}} ) {
	push (@encls, $val);
	if ( $display != 1 ) {
		if (($val !~ "description") && ($val !~ "serial_number") && !(($mapping_mode == 2) && ($val =~ "slot"))) {
			push (@allowed_nums, $num);
			print " $allowed_num.\t$val\n";
			$allowed_num++;
		}
	} else {
		if (!(($mapping_mode == 2) && ($val =~ "slot")) && !(($mapping_mode == 1) && ($val =~ "serial_number"))) {
			push (@allowed_nums, $num);
			print " $allowed_num.\t$val\n";
			$allowed_num++;
		}
	}
	$num++;
    }
    $num--;
    $allowed_num--;
    $encl_max = $num;
    print "\n";
    print "Select an Attribute:  [1-$allowed_num or 0 to quit] ";
    $option = <>;
    chomp ($option);
    if($option !~ m/\d/) {
	print "Invalid Option Selected.\n";
	$option = 0;
    }
    if($option == 0) {
	return $option-1;
    }
    $option = $allowed_nums[$option-1];
    return $option-1;
}

sub Display_Info
{
    my ($curr_hba) = @_;
    my $attr =  List_Attr($curr_hba);
    my $attr_val;
    my $encl;

    while ($attr > -1)
    {
	if($attr < $attr_max) {
	    if (-d "/config/mpt3sas/$hbas[$curr_hba]/$attrs[$attr]")
	    {
		$encl = List_Encl($curr_hba, $attr, 1);
		while ($encl > -1)
		{
		    if($encl < $encl_max) {
			if(-e "/config/mpt3sas/$hbas[$curr_hba]/$attrs[$attr]/$encls[$encl]") {
			    open (ATTR_FH, "/config/mpt3sas/$hbas[$curr_hba]/$attrs[$attr]/$encls[$encl]");
			    $attr_val = <ATTR_FH>;
			    chomp ($attr_val);
			    if($mpt3sas{$hbas[$curr_hba]}{$attrs[$attr]}{$encls[$encl]} ne $attr_val)
			    {
				Update_HASH_encl($curr_hba,$attr,$encl,$attr_val);
			    }
			    print "$encls[$encl] = $attr_val\n";
			    close (ATTR_FH);
			} else	{
			    Exit_Program
			}
		    } else {
			print "Invalid Option Selected.\n";
		    }
		    $encl = List_Encl($curr_hba, $attr, 1);
		}
	    }
	    elsif (-e "/config/mpt3sas/$hbas[$curr_hba]/$attrs[$attr]")
	    {
		open (ATTR_FH, "/config/mpt3sas/$hbas[$curr_hba]/$attrs[$attr]");
		$attr_val = <ATTR_FH>;
		chomp ($attr_val);
		if($mpt3sas{$hbas[$curr_hba]}{$attrs[$attr]} ne $attr_val)
		{
		    Update_HASH_attr($curr_hba,$attr,$attr_val);
		}
		print "$attrs[$attr] = $attr_val\n";
		close (ATTR_FH);
	    } else {
		Exit_Program
	    }
	}
	else {
	    print "Invalid Option Selected.\n";
	}
	$attr =  List_Attr($curr_hba);
    }
}

sub Write_Info
{
    my ($curr_hba) = @_;
    my $attr =  List_Attr($curr_hba);
    my $attr_val;
    my $encl;
    my $ret;

    while ($attr > -1)
    {
	if($attr < $attr_max) {
	    if (-d "/config/mpt3sas/$hbas[$curr_hba]/$attrs[$attr]") {
		$encl = List_Encl($curr_hba, $attr, 0);
		while ($encl > -1 ) {
		    if($encl < $encl_max) {
			if(-e "/config/mpt3sas/$hbas[$curr_hba]/$attrs[$attr]/$encls[$encl]") {
			    	print "Enter the value for $encls[$encl] : ";
			    	$attr_val = <>;
			    	chomp ($attr_val);
			    	`echo $attr_val > /config/mpt3sas/$hbas[$curr_hba]/$attrs[$attr]/$encls[$encl]`;
			    	$ret = `echo $?`;
			    	if ($ret != 0) {
					print "Error while writing $attr_val\n";
					Exit_Program;
				}
				Update_HASH_encl($curr_hba,$attr,$encl,$attr_val);
		        } else {
			    Exit_Program
		        } 
		    }
		    else {
			print "Invalid Option Selected.\n";
		    }
		    $encl = List_Encl($curr_hba, $attr, 0);
		}
	    }
	    elsif (-e "/config/mpt3sas/$hbas[$curr_hba]/$attrs[$attr]")
	    {
		open (ATTR_FH, ">/config/mpt3sas/$hbas[$curr_hba]/$attrs[$attr]");
		if ($attrs[$attr] =~ "mapping_mode") {
		    print "Enter the value for $attrs[$attr] (1 - Enclosure Slot Mapping, 2 - Persistent Device Mapping) : ";
		} else {
		    print "Enter the value for $attrs[$attr] : ";
		}
		$attr_val = <>;
		chomp ($attr_val);
		`echo $attr_val > /config/mpt3sas/$hbas[$curr_hba]/$attrs[$attr]`;
		$ret = `echo $?`;
		if ($ret != 0) {
		    print "Error while writing $attr_val\n";
		    Exit_Program;
		}
		Update_HASH_attr($curr_hba,$attr,$attr_val);
	    }
	    else
	    {
		Exit_Program
	    }
	}       else {
	    print "Invalid Option Selected.\n";
	}

	$attr =  List_Attr($curr_hba);
    }
}

sub Create_Enclosure_Slot
{
    my ($curr_hba) = @_;
    my $encl;
    my $slot;
    my $ret;
    my @directory = ();

    print "Enter the Name to identify the disk : ";
    $encl = <>;
    chomp($encl);
    `mkdir /config/mpt3sas/$hbas[$curr_hba]/$encl`;
    $ret = `echo $?`;
    if ($ret != 0)
    {
	print "Error while creating disk directory\n";
	Exit_Program;
    }
    our $rec1 = {};
    $mpt3sas{$hbas[$curr_hba]}{$encl} = $rec1;
    push (@directory, "/config/mpt3sas/$hbas[$curr_hba]/$encl");
    find(\&Slurp_Directory, @directory);
    $hash_modified = 1;
}

sub Delete_Enclosure_Slot
{
    my ($curr_hba) = @_;
    my $encl;
    my $num = 1;
    my $opt;
    my $ret;
    my @enclosure = ();

    print "Disk Mappings present for $hbas[$curr_hba]\n";
    foreach $encl ( sort keys %{ $mpt3sas{$hbas[$curr_hba]} } ) {
	if(($encl !~ "ioc_number") && ($encl !~ "mapping_mode") &&
		   ($encl !~ "description")) {
	    print "$num. $encl\n";
	    push(@enclosure, $encl);
	    $num++;
	}
    }
    $num--;
    print "Select the mapped disk [1 to $num] : ";
    $opt = <>;
    chomp($opt);
    `rmdir /config/mpt3sas/$hbas[$curr_hba]/$enclosure[$opt-1]`;
    $ret = `echo $?`;
    if ($ret != 0)
    {
	print "Error while deleting Enclosure Slot directory\n";
	Exit_Program;
    }
    delete($mpt3sas{$hbas[$curr_hba]}{$enclosure[$opt-1]});
    $hash_modified = 1;
}

sub List_Select_HBA
{
    my $option;
    my $num = 1;

    print "LSI Logic MPT3SAS Configfs Utility, Version 0.02, February 5, 2015\n";
    print "$no_of_hba HBAs found\n\n";
    print "\tHBA Name\n";
    foreach my $hba ( sort keys %mpt3sas ) {
	if( $hba =~ "hba") {
	    print "  $num\.\t/config/mpt3sas/$hba\n";
	    $num++;
	}
    }
    print "\n";
    print "Select a device:  [1-$no_of_hba or 0 to quit] ";
    $option = <>;
    chomp($option);
    if(($option !~ m/\d/) || ($option == 0))
    {
	Exit_Program;
    }
    return $option - 1;
}

sub List_Select_Action
{
    my $option;
    print "\n";
    print "1. Display Information\n";
    print "2. Update Information\n";
    print "3. Create Visibility for a disk\n";
    print "4. Delete Visibility for a disk\n";
    print "\n";
    print "Main menu, select an option:  [1-4 or 0 to quit] ";
    $option = <>;
    chomp($option);
    if($option !~ m/\d/)
    {
	print "Invalid Option Selected.\n";
	$option = 0;
    }
    return $option;
}

sub Select_Action
{
    my $option;
    print "\n";
    print "Main menu, select an option:  [1-4 or 0 to quit] ";
    $option = <>;
    chomp($option);
    if($option !~ m/\d/)
    {
	$option = List_Select_Action;
    }
    return $option;
}

sub Interact_Interface
{
    my $curr_hba;
    my $action;
    my %action_to_take = (
	'0' => \&Exit_Program,
	'1' => \&Display_Info,
	'2' => \&Write_Info,
	'3' => \&Create_Enclosure_Slot,
	'4' => \&Delete_Enclosure_Slot,
	);

    Push_HBAs_In_Array;
    $curr_hba = List_Select_HBA;
    while (($curr_hba > -1) && ($curr_hba < $no_of_hba))
    {
	$action = List_Select_Action;
	while ($action > 0)
	{
	    if (defined $action_to_take{$action}) {
		$action_to_take{$action}->($curr_hba);
	    }
	    else
	    {
		print "Invalid Option Selected.\n";
	    }
	    $action = Select_Action;
	}
	$curr_hba = List_Select_HBA;
    }
}

if(($#ARGV == 0) && ($ARGV[0] =~ "discover"))
{
    Start_Discovery;
    Create_XML_File;
}
elsif (($#ARGV == 0) && ($ARGV[0] =~ "start"))
{
    Recreate_files_from_XML_config;
}
elsif (($#ARGV == 0) && ($ARGV[0] =~ "stop"))
{
    Read_XML_File;
    Write_Internally_Modified_Attributes_to_XML;
    if($hash_modified == 1) {
        Create_XML_File
    }
    Cleanup_Mapping;
}
elsif($#ARGV < 0)
{
    Read_XML_File;
    Interact_Interface;
}
