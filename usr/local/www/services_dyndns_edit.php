<?php
/* $Id$ */
/*
	Copyright (C) 2008 Ermal Lu�i
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
*/
/*
	pfSense_BUILDER_BINARIES:	/bin/rm
	pfSense_MODULE:	dyndns
*/

##|+PRIV
##|*IDENT=page-services-dynamicdnsclient
##|*NAME=Services: Dynamic DNS client page
##|*DESCR=Allow access to the 'Services: Dynamic DNS client' page.
##|*MATCH=services_dyndns_edit.php*
##|-PRIV

/* returns true if $uname is a valid DynDNS username */
function is_dyndns_username($uname) {
        if (!is_string($uname))
                return false;
        
        if (preg_match("/[^a-z0-9\-.@_:]/i", $uname))
                return false;
        else
                return true;
}

require("guiconfig.inc");

if (!is_array($config['dyndnses']['dyndns'])) {
	$config['dyndnses']['dyndns'] = array();
}

$a_dyndns = &$config['dyndnses']['dyndns'];

$id = $_GET['id'];
if (isset($_POST['id']))
	$id = $_POST['id'];

if (isset($id) && isset($a_dyndns[$id])) {
	$pconfig['username'] = $a_dyndns[$id]['username'];
	$pconfig['password'] = $a_dyndns[$id]['password'];
	$pconfig['host'] = $a_dyndns[$id]['host'];
	$pconfig['mx'] = $a_dyndns[$id]['mx'];
	$pconfig['type'] = $a_dyndns[$id]['type'];
	$pconfig['enable'] = !isset($a_dyndns[$id]['enable']);
	$pconfig['interface'] = $a_dyndns[$id]['interface'];
	$pconfig['wildcard'] = isset($a_dyndns[$id]['wildcard']);
	$pconfig['descr'] = $a_dyndns[$id]['descr'];
}

if ($_POST) {

	unset($input_errors);
	$pconfig = $_POST;

	/* input validation */
	$reqdfields = array();
	$reqdfieldsn = array();
	$reqdfields = array_merge($reqdfields, explode(" ", "host username password type"));
	$reqdfieldsn = array_merge($reqdfieldsn, explode(",", "Hostname,Username,Password,Service type"));

	do_input_validation($_POST, $reqdfields, $reqdfieldsn, &$input_errors);

	if (($_POST['mx'] && !is_domain($_POST['mx']))) 
		$input_errors[] = "The MX contains invalid characters.";
	if (($_POST['username'] && !is_dyndns_username($_POST['username'])) || $_POST['username'] == "") 
		$input_errors[] = "The username contains invalid characters.";

	if (!$input_errors) {
		$dyndns = array();
		$dyndns['type'] = $_POST['type'];
		$dyndns['username'] = $_POST['username'];
		$dyndns['password'] = $_POST['password'];
		$dyndns['host'] = $_POST['host'];
		$dyndns['mx'] = $_POST['mx'];
		$dyndns['wildcard'] = $_POST['wildcard'] ? true : false;
		$dyndns['enable'] = $_POST['enable'] ? false : true;
		$dyndns['interface'] = $_POST['interface'];
		$dyndns['descr'] = $_POST['descr'];

		if (isset($id) && $a_dyndns[$id])
			$a_dyndns[$id] = $dyndns;
		else
			$a_dyndns[] = $dyndns;

		write_config();


		/* XXX: Make this with a touch file */
		$retval = 0;

		conf_mount_rw();
		mwexec("/bin/rm {$g['conf_path']}/dyndns_{$dyndns['interface']}{$dyndns['type']}.cache");
		conf_mount_ro();

		$retval = services_dyndns_configure_client($dyndns);

		header("Location: services_dyndns.php");
		exit;
	}
}

$pgtitle = array("Services","Dynamic DNS client");
include("head.inc");

?>

<body link="#0000CC" vlink="#0000CC" alink="#0000CC">
<?php include("fbegin.inc"); ?>
<?php if ($input_errors) print_input_errors($input_errors); ?>
<?php if ($savemsg) print_info_box($savemsg); ?>
<form action="services_dyndns_edit.php" method="post" name="iform" id="iform">
              <table width="100%" border="0" cellpadding="6" cellspacing="0">
                <tr>
                  <td colspan="2" valign="top" class="optsect_t">
				  <table border="0" cellspacing="0" cellpadding="0" width="100%">
				  <tr><td class="optsect_s"><strong>Dynamic DNS client</strong></td></tr>
				  </table>
				  </td>
                </tr>
                <tr>
                  <td width="22%" valign="top" class="vncellreq">Disable</td>
				  <td width="78%" class="vtable">
				    <input name="enable" type="checkbox" id="enable" value="yes" <?php if ($pconfig['enable']) echo "checked"; ?>>
				  </td>
                </tr>
                <tr>
                  <td width="22%" valign="top" class="vncellreq">Service type</td>
                  <td width="78%" class="vtable">
			<select name="type" class="formselect" id="type">
                      <?php
						$types = explode(",", "DNS-O-Matic, DynDNS (dynamic),DynDNS (static),DynDNS (custom),DHS,DyNS,easyDNS,No-IP,ODS.org,ZoneEdit,Loopia,freeDNS, DNSexit, OpenDNS");
						$vals = explode(" ", "dnsomatic dyndns dyndns-static dyndns-custom dhs dyns easydns noip ods zoneedit loopia freedns dnsexit opendns");
						$j = 0; for ($j = 0; $j < count($vals); $j++): ?>
                      <option value="<?=$vals[$j];?>" <?php if ($vals[$j] == $pconfig['type']) echo "selected";?>>
                      <?=htmlspecialchars($types[$j]);?>
                      </option>
                      <?php endfor; ?>
                    </select></td>
				</tr>
				<tr>
				   <td width="22%" valign="top" class="vncellreq">Interface to monitor</td>  
				   <td width="78%" class="vtable">
				   <select name="interface" class="formselect" id="interface">
				   <?php $iflist = get_configured_interface_with_descr();
				   		foreach ($iflist as $if => $ifdesc):?>
							<option value="<?=$if;?>" <?php if ($pconfig['interface'] == $if) echo "selected";?>><?=$ifdesc;?></option>
					<?php endforeach; ?>
					</select>
					</td>
					</td>
				</tr>	
                <tr>
                  <td width="22%" valign="top" class="vncellreq">Hostname/Interface</td>
                  <td width="78%" class="vtable">
                    <input name="host" type="text" class="formfld unknown" id="host" size="30" value="<?=htmlspecialchars($pconfig['host']);?>">
                    <br>
				    <span class="vexpl">
				    <span class="red"><strong>Note:<br></strong>
				    </span>
					Enter the complete host/domain name.  example:  myhost.dyndns.org
				    </span>
		          </td>
				</tr>
                <tr>
                  <td width="22%" valign="top" class="vncell">MX</td>
                  <td width="78%" class="vtable">
                    <input name="mx" type="text" class="formfld unknown" id="mx" size="30" value="<?=htmlspecialchars($pconfig['mx']);?>">
                    <br>
					Note: With DynDNS service you can only use a hostname, not an IP address.
					<br>
                    Set this option only if you need a special MX record. Not
                    all services support this.</td>
				</tr>
                <tr>
                  <td width="22%" valign="top" class="vncellreq">Wildcards</td>
                  <td width="78%" class="vtable">
                    <input name="wildcard" type="checkbox" id="wildcard" value="yes" <?php if ($pconfig['wildcard']) echo "checked"; ?>>
                    Enable Wildcard</td>
				</tr>
                <tr>
                  <td width="22%" valign="top" class="vncellreq">Username</td>
                  <td width="78%" class="vtable">
                    <input name="username" type="text" class="formfld user" id="username" size="20" value="<?=htmlspecialchars($pconfig['username']);?>">
                  </td>
                </tr>
                <tr>
                  <td width="22%" valign="top" class="vncellreq">Password</td>
                  <td width="78%" class="vtable">
                    <input name="password" type="password" class="formfld pwd" id="password" size="20" value="<?=htmlspecialchars($pconfig['password']);?>">
                  </td>
                </tr>
                <tr>
                  <td width="22%" valign="top" class="vncellreq">Description</td>
                  <td width="78%" class="vtable">
                    <input name="descr" type="text" class="formfld unknown" id="descr" size="60" value="<?=htmlspecialchars($pconfig['descr']);?>">
                  </td>
                </tr>
                <tr>
                  <td width="22%" valign="top">&nbsp;</td>
                  <td width="78%">
                    <input name="Submit" type="submit" class="formbtn" value="Save" onClick="enable_change(true)">
					<a href="services_dyndns.php"><input name="cancel" type="button" class="formbtn" value="Cancel"></a>
					<?php if (isset($id) && $a_dyndns[$id]): ?>
						<input name="id" type="hidden" value="<?=$id;?>">
					<?php endif; ?>
                  </td>
                </tr>
                <tr>
                  <td width="22%" valign="top">&nbsp;</td>
                  <td width="78%"><span class="vexpl"><span class="red"><strong>Note:<br>
                    </strong></span>You must configure a DNS server in <a href="system.php">System:
                    General setup</a> or allow the DNS server list to be overridden
                    by DHCP/PPP on WAN for dynamic DNS updates to work.</span></td>
                </tr>
              </table>
</form>
<?php include("fend.inc"); ?>
</body>
</html>