<?php
/****h* pfSense/config
 * NAME
 *   config.inc - Functions to manipulate config.xml
 * DESCRIPTION
 *   This include contains various config.xml specific functions.
 * HISTORY
 * $Id$
 ******

	config.console.inc
	Copyright (C) 2004-2010 Scott Ullrich
	All rights reserved.

	originally part of m0n0wall (http://m0n0.ch/wall)
	Copyright (C) 2003-2004 Manuel Kasper <mk@neon1.net>.
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.


	pfSense_BUILDER_BINARIES:	/sbin/mount	/sbin/sysctl	/sbin/umount	/sbin/halt	/sbin/fsck	/bin/sync
	pfSense_MODULE:	config
*/

function set_networking_interfaces_ports() {
	global $noreboot;
	global $config;
	global $g;
	global $fp;

	$fp = fopen('php://stdin', 'r');

	$memory = get_memory();
	$avail = $memory[0];

	if($avail < $g['minimum_ram_warning']) {
		echo "\n\n\n";
		echo "DANGER!  WARNING!  ACHTUNG!\n\n";
		echo "{$g['product_name']} requires *AT LEAST* {$g['minimum_ram_warning_text']} RAM to function correctly.\n";
		echo "Only ({$avail}) MB RAM has been detected.\n";
		echo "\nPress ENTER to continue. ";
		fgets($fp);
		echo "\n";
	}

	$iflist = get_interface_list();

/* Function flow is based on $key and $auto_assign or the lack thereof */	
	$key = null;

/* Only present auto interface option if running from LiveCD and interface mismatch*/
	if ((ereg("cdrom", $g['platform'])) && is_interface_mismatch())
		$auto_assign = false;

	echo <<<EOD

Valid interfaces are:


EOD;

	if(!is_array($iflist)) {
		echo "No interfaces found!\n";
		$iflist = array();
	} else {
		foreach ($iflist as $iface => $ifa) {
			echo sprintf("% -6s%s%s\t%s\n", $iface, $ifa['mac'],
				$ifa['up'] ? "   (up)" : "   (down)", $ifa['dmesg']);
		}
	}

	if ($auto_assign) {
		echo <<<EOD
		
		!!! LiveCD Detected: Auto Interface Option !!!!
BEGIN MANUAL CONFIGURATION OR WE WILL PROCEED WITH AUTO CONFIGURATION.

EOD;
	}	
	
	echo <<<EOD

Do you want to set up VLANs first? 

If you are not going to use VLANs, or only for optional interfaces, you should
say no here and use the webConfigurator to configure VLANs later, if required.

Do you want to set up VLANs now [y|n]? 
EOD;

	if ($auto_assign) {
		$key = timeout();

	} else
		$key = chop(fgets($fp));

	if (!isset($key) and $auto_assign) {	// Auto Assign Interfaces
		do {
			echo <<<EOD

   !!! Auto Assigning Interfaces !!!

For installation purposes, you must plug in at least one NIC
for the LAN connection. If you plug in a second NIC it will be
assigned to WAN. Otherwise, we'll temporarily assign WAN to the
next available NIC found regardless of activity. You should
assign and configure the WAN interface according to your requirements

If you haven't plugged in any network cables yet,
now is the time to do so.
We'll keep trying until you do.

Searching for active interfaces...
 
EOD;
			unset($wanif, $lanif);

			$media_iflist = $plugged_in = array();
			$media_iflist = get_interface_list("media");
			foreach ($media_iflist as $iface => $ifa) {
				if ($ifa['up']) 
					$plugged_in[] = $iface;
				
			}

			$lanif = array_shift($plugged_in);
			$wanif = array_shift($plugged_in);

			if(isset($lanif) && !isset($wanif)) {
				foreach ($iflist as $iface => $ifa) {
					if ($iface != $lanif) {
						$wanif = $iface;
						break;
					}
				}
			}

			echo <<<EOD

Assigned WAN to : $wanif 
Assigned LAN to : $lanif

If you don't like this assignment,
press any key to go back to manual configuration. 

EOD;
			$key = timeout(20);
			if(isset($key))
				return;
		} while (!isset($wanif));

		$config['system']['enablesshd'] = 'enabled';	
		$key = 'y';

	} else {		//Manually assign interfaces	
		if (in_array($key, array('y', 'Y')))
			vlan_setup();
	
		if (is_array($config['vlans']['vlan']) && count($config['vlans']['vlan'])) {
	
			echo "\n\nVLAN interfaces:\n\n";
			foreach ($config['vlans']['vlan'] as $vlan) {
	
				echo sprintf("% -16s%s\n", "{$vlan['if']}_vlan{$vlan['tag']}",
					"VLAN tag {$vlan['tag']}, parent interface {$vlan['if']}");
	
				$iflist[$vlan['if'] . '_vlan' . $vlan['tag']] = array();
			}
		}
	
		echo <<<EOD
	
*NOTE*  {$g['product_name']} requires {$g['minimum_nic_count_text']} assigned interfaces to function.
        If you do not have {$g['minimum_nic_count_text']} interfaces you CANNOT continue. 

        If you do not have at least {$g['minimum_nic_count']} *REAL* network interface cards
        or one interface with multiple VLANs then {$g['product_name']}
        *WILL NOT* function correctly.

If you do not know the names of your interfaces, you may choose to use
auto-detection. In that case, disconnect all interfaces now before
hitting 'a' to initiate auto detection.
	
EOD;
	
		do {
			echo "\nEnter the WAN interface name or 'a' for auto-detection: ";
			$wanif = chop(fgets($fp));
			if ($wanif === "") {
				return;
			}
			if ($wanif === "a")
				$wanif = autodetect_interface("WAN", $fp);
			else if (!array_key_exists($wanif, $iflist)) {
				echo "\nInvalid interface name '{$wanif}'\n";
				unset($wanif);
				continue;
			}
		} while (!$wanif);
	
		do {
			echo "\nEnter the LAN interface name or 'a' for auto-detection \n" .
			    "NOTE: this enables full Firewalling/NAT mode.\n" .
				"(or nothing if finished): ";
	
			$lanif = chop(fgets($fp));
			
			if($lanif == "exit") {
				exit;
			}
			
			if($lanif == "") {
				if($g['minimum_nic_count'] < 2) {
					break;	
				} else {
					fclose($fp);
					return;
				}
			}
	
			if ($lanif === "a")
				$lanif = autodetect_interface("LAN", $fp);
			else if (!array_key_exists($lanif, $iflist)) {
				echo "\nInvalid interface name '{$lanif}'\n";
				unset($lanif);
				continue;
			}
		} while (!$lanif);
	
		/* optional interfaces */
		$i = 0;
		$optif = array();
	
		if($lanif <> "") {
			while (1) {
				if ($optif[$i])
					$i++;
				$i1 = $i + 1;
		
				if($config['interfaces']['opt' . $i1]['descr'])
					echo "\nOptional interface {$i1} description found: {$config['interfaces']['opt' . $i1]['descr']}";
	
				echo "\nEnter the Optional {$i1} interface name or 'a' for auto-detection\n" .
					"(or nothing if finished): ";
		
				$optif[$i] = chop(fgets($fp));
		
				if ($optif[$i]) {
					if ($optif[$i] === "a") {
						$ad = autodetect_interface("Optional " . $i1, $fp);
						if ($ad)
							$optif[$i] = $ad;
						else
							unset($optif[$i]);
					} else if (!array_key_exists($optif[$i], $iflist)) {
						echo "\nInvalid interface name '{$optif[$i]}'\n";
						unset($optif[$i]);
						continue;
					}
				} else {
					unset($optif[$i]);
					break;
				}
			}
		}
		
		/* check for double assignments */
		$ifarr = array_merge(array($lanif, $wanif), $optif);
		
		for ($i = 0; $i < (count($ifarr)-1); $i++) {
			for ($j = ($i+1); $j < count($ifarr); $j++) {
				if ($ifarr[$i] == $ifarr[$j]) {
					echo <<<EOD
	
Error: you cannot assign the same interface name twice!
	
EOD;
					fclose($fp);
					return;
				}
			}
		}
	
		echo "\nThe interfaces will be assigned as follows: \n\n";
	
		if ($lanif != "")
			echo "LAN  -> " . $lanif . "\n";
		echo "WAN  -> " . $wanif . "\n";
		for ($i = 0; $i < count($optif); $i++) {
			echo "OPT" . ($i+1) . " -> " . $optif[$i] . "\n";
		}
	
		echo <<<EOD
	
Do you want to proceed [y|n]?
EOD;
			$key = chop(fgets($fp));		
	}

	if (in_array($key, array('y', 'Y'))) {
		if($lanif) {
			$config['interfaces']['lan']['if'] = $lanif;
			$config['interfaces']['lan']['enable'] = true;
		} elseif (!$g['booting'] && !$auto_assign) {

echo <<<EODD

You have chosen to remove the LAN interface.

Would you like to remove the LAN IP address and
unload the interface now? [y|n]? 
EODD;

				if (strcasecmp(chop(fgets($fp)), "y") == 0) {
					if($config['interfaces']['lan']['if'])
						mwexec("/sbin/ifconfig " . $config['interfaces']['lan']['if'] . " delete");
				}
				if(isset($config['interfaces']['lan']))
					unset($config['interfaces']['lan']);
				if(isset($config['dhcpd']['lan']))
					unset($config['dhcpd']['lan']);
				if(isset($config['interfaces']['lan']['if']))
					unset($config['interfaces']['lan']['if']);
				if(isset($config['interfaces']['wan']['blockpriv']))
					unset($config['interfaces']['wan']['blockpriv']);
				if(isset($config['shaper']))
					unset($config['shaper']);
				if(isset($config['ezshaper']))
					unset($config['ezshaper']);
				if(isset($config['nat']))
					unset($config['nat']);				
		} else {
			if(isset($config['interfaces']['lan']['if']))
				mwexec("/sbin/ifconfig " . $config['interfaces']['lan']['if'] . " delete");
			if(isset($config['interfaces']['lan']))
				unset($config['interfaces']['lan']);
			if(isset($config['dhcpd']['lan']))
				unset($config['dhcpd']['lan']);
			if(isset($config['interfaces']['lan']['if']))
				unset($config['interfaces']['lan']['if']);
			if(isset($config['interfaces']['wan']['blockpriv']))
				unset($config['interfaces']['wan']['blockpriv']);
			if(isset($config['shaper']))
				unset($config['shaper']);
			if(isset($config['ezshaper']))
				unset($config['ezshaper']);
			if(isset($config['nat']))
				unset($config['nat']);				
		}
		if (preg_match($g['wireless_regex'], $lanif)) {
			if (is_array($config['interfaces']['lan']) &&
				(!is_array($config['interfaces']['lan']['wireless'])))
				$config['interfaces']['lan']['wireless'] = array();
		} else {
			unset($config['interfaces']['lan']['wireless']);
		}

		$config['interfaces']['wan']['if'] = $wanif;
		$config['interfaces']['wan']['enable'] = true;
		if (preg_match($g['wireless_regex'], $wanif)) {
			if (is_array($config['interfaces']['wan']) &&
				(!is_array($config['interfaces']['wan']['wireless'])))
				$config['interfaces']['wan']['wireless'] = array();
		} else {
			unset($config['interfaces']['wan']['wireless']);
		}

		for ($i = 0; $i < count($optif); $i++) {
			if (!is_array($config['interfaces']['opt' . ($i+1)]))
				$config['interfaces']['opt' . ($i+1)] = array();

			$config['interfaces']['opt' . ($i+1)]['if'] = $optif[$i];

			/* wireless interface? */
			if (preg_match($g['wireless_regex'], $optif[$i])) {
				if (!is_array($config['interfaces']['opt' . ($i+1)]['wireless']))
					$config['interfaces']['opt' . ($i+1)]['wireless'] = array();
			} else {
				unset($config['interfaces']['opt' . ($i+1)]['wireless']);
			}

			unset($config['interfaces']['opt' . ($i+1)]['enable']);
			$config['interfaces']['opt' . ($i+1)]['descr'] = "OPT" . ($i+1);
		}

		/* remove all other (old) optional interfaces */
		for (; isset($config['interfaces']['opt' . ($i+1)]); $i++)
			unset($config['interfaces']['opt' . ($i+1)]);

		echo "\nWriting configuration...";
		write_config();
		echo "done.\n";

		fclose($fp);

		if($g['booting'])
			return;

		echo "One moment while we reload the settings...";

		$g['booting'] = false;

		/* XXX: ermal - disable it for now this is used during bootup at best so shouldn't be needed.
		 * 		For now just comment it out and later remove it completely.
		 * resync everything 
			reload_all_sync();
		 */

		echo " done!\n";

		touch("{$g['tmp_path']}/assign_complete");

	}
}

function autodetect_interface($ifname, $fp) {
	$iflist_prev = get_interface_list("media");
	echo <<<EOD

Connect the {$ifname} interface now and make sure that the link is up.
Then press ENTER to continue.

EOD;
	fgets($fp);
	$iflist = get_interface_list("media");

	foreach ($iflist_prev as $ifn => $ifa) {
		if (!$ifa['up'] && $iflist[$ifn]['up']) {
			echo "Detected link-up on interface {$ifn}.\n";
			return $ifn;
		}
	}

	echo "No link-up detected.\n";

	return null;
}

function interfaces_setup() {
	global $iflist, $config, $g, $fp;

	$iflist = get_interface_list();

	
}

function vlan_setup() {
	global $iflist, $config, $g, $fp;

	$iflist = get_interface_list();

	if (is_array($config['vlans']['vlan']) && count($config['vlans']['vlan'])) {

	echo <<<EOD

WARNING: all existing VLANs will be cleared if you proceed!

Do you want to proceed [y|n]?
EOD;

	if (strcasecmp(chop(fgets($fp)), "y") != 0)
		return;
	}

	$config['vlans']['vlan'] = array();
	echo "\n";

	$vlanif = 0;

	while (1) {
		$vlan = array();

		echo "\n\nVLAN Capable interfaces:\n\n";
		if(!is_array($iflist)) {
			echo "No interfaces found!\n";
		} else {
			$vlan_capable=0;
			foreach ($iflist as $iface => $ifa) {
				if (is_jumbo_capable($iface)) {
					echo sprintf("% -8s%s%s\n", $iface, $ifa['mac'],
						$ifa['up'] ? "   (up)" : "");
					$vlan_capable++;
				}
			}
		}

		if($vlan_capable == 0) {
			echo "No VLAN capable interfaces detected.\n";
			return;
		}

		echo "\nEnter the parent interface name for the new VLAN (or nothing if finished): ";
		$vlan['if'] = chop(fgets($fp));

		if ($vlan['if']) {
			if (!array_key_exists($vlan['if'], $iflist) or
			    !is_jumbo_capable($vlan['if'])) {
				echo "\nInvalid interface name '{$vlan['if']}'\n";
				continue;
			}
		} else {
			break;
		}

		echo "Enter the VLAN tag (1-4094): ";
		$vlan['tag'] = chop(fgets($fp));
		$vlan['vlanif'] = "{$vlan['if']}_vlan{$vlan['tag']}";
		if (!is_numericint($vlan['tag']) || ($vlan['tag'] < 1) || ($vlan['tag'] > 4094)) {
			echo "\nInvalid VLAN tag '{$vlan['tag']}'\n";
			continue;
		}
		
		$config['vlans']['vlan'][] = $vlan;
		$vlanif++;
	}
}

?>