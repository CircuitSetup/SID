/*
 * -------------------------------------------------------------------
 * CircuitSetup.us Status Indicator Display
 * (C) 2023-2025 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/SID
 * https://sid.out-a-ti.me
 *
 * WiFi and Config Portal handling
 *
 * -------------------------------------------------------------------
 * License: MIT NON-AI
 * 
 * Permission is hereby granted, free of charge, to any person 
 * obtaining a copy of this software and associated documentation 
 * files (the "Software"), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, 
 * merge, publish, distribute, sublicense, and/or sell copies of the 
 * Software, and to permit persons to whom the Software is furnished to 
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be 
 * included in all copies or substantial portions of the Software.
 *
 * In addition, the following restrictions apply:
 * 
 * 1. The Software and any modifications made to it may not be used 
 * for the purpose of training or improving machine learning algorithms, 
 * including but not limited to artificial intelligence, natural 
 * language processing, or data mining. This condition applies to any 
 * derivatives, modifications, or updates based on the Software code. 
 * Any usage of the Software in an AI-training dataset is considered a 
 * breach of this License.
 *
 * 2. The Software may not be included in any dataset used for 
 * training or improving machine learning algorithms, including but 
 * not limited to artificial intelligence, natural language processing, 
 * or data mining.
 *
 * 3. Any person or organization found to be in violation of these 
 * restrictions will be subject to legal action and may be held liable 
 * for any damages resulting from such use.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "sid_global.h"

#include <Arduino.h>

#ifdef SID_MDNS
#include <ESPmDNS.h>
#endif

#include "src/WiFiManager/WiFiManager.h"

#include "sid_settings.h"
#include "sid_wifi.h"
#include "sid_main.h"
#ifdef SID_HAVEMQTT
#include "mqtt.h"
#endif

// If undefined, use the checkbox/dropdown-hacks.
// If defined, go back to standard text boxes
//#define TC_NOCHECKBOXES

Settings settings;

IPSettings ipsettings;

WiFiManager wm;
bool wifiSetupDone = false;

#ifdef SID_HAVEMQTT
WiFiClient mqttWClient;
PubSubClient mqttClient(mqttWClient);
#endif

static const char *aco = "autocomplete='off'";

#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
WiFiManagerParameter custom_bootSA("bSA", "Boot into Spectrum Analyzer (0=no, 1=yes)", settings.bootSA, 1, "autocomplete='off'");
#else // -------------------- Checkbox hack: --------------
WiFiManagerParameter custom_bootSA("bSA", "Boot into Spectrum Analyzer", settings.bootSA, 1, "type='checkbox' style='margin-top:5px;'", WFM_LABEL_AFTER);
#endif // -------------------------------------------------
WiFiManagerParameter custom_ssDelay("ssDel", "<br>Screen Saver timer (minutes; 0=off)", settings.ssTimer, 3, "type='number' min='0' max='999' autocomplete='off'", WFM_LABEL_BEFORE);

#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
WiFiManagerParameter custom_disDIR("dDIR", "Disable supplied IR control (0=off, 1=on)", settings.disDIR, 1, "autocomplete='off' title='Set to 1 to disable the supplied IR remote control'");
#else // -------------------- Checkbox hack: --------------
WiFiManagerParameter custom_disDIR("dDIR", "Disable supplied IR control", settings.disDIR, 1, "title='Check to disable the supplied IR remote control' type='checkbox' style='margin-top:5px;'", WFM_LABEL_AFTER);
#endif // -------------------------------------------------

#if defined(SID_MDNS) || defined(SID_WM_HAS_MDNS)
#define HNTEXT "Hostname<br><span style='font-size:80%'>The Config Portal is accessible at http://<i>hostname</i>.local<br>(Valid characters: a-z/0-9/-)</span>"
#else
#define HNTEXT "Hostname<br><span style='font-size:80%'>(Valid characters: a-z/0-9/-)</span>"
#endif
WiFiManagerParameter custom_hostName("hostname", HNTEXT, settings.hostName, 31, "pattern='[A-Za-z0-9\\-]+' placeholder='Example: sid'");
WiFiManagerParameter custom_sysID("sysID", "AP Mode: Network name appendix<br><span style='font-size:80%'>Will be appended to \"SID-AP\" to create a unique name if multiple SIDs in range. [a-z/0-9/-]</span>", settings.systemID, 7, "pattern='[A-Za-z0-9\\-]+'");
WiFiManagerParameter custom_appw("appw", "AP Mode: WiFi password<br><span style='font-size:80%'>Password to protect SID-AP. Empty or 8 characters [a-z/0-9/-]<br><b>Write this down, you might lock yourself out!</b></span>", settings.appw, 8, "minlength='8' pattern='[A-Za-z0-9\\-]+'");
WiFiManagerParameter custom_wifiConRetries("wifiret", "WiFi connection attempts (1-10)", settings.wifiConRetries, 2, "type='number' min='1' max='10' autocomplete='off'", WFM_LABEL_BEFORE);
WiFiManagerParameter custom_wifiConTimeout("wificon", "WiFi connection timeout (7-25[seconds])", settings.wifiConTimeout, 2, "type='number' min='7' max='25'");

#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
WiFiManagerParameter custom_TCDpresent("TCDpres", "TCD connected by wire (0=no, 1=yes)", settings.TCDpresent, 1, "autocomplete='off' title='Enable if you have a Time Circuits Display connected via wire'");
#else // -------------------- Checkbox hack: --------------
WiFiManagerParameter custom_TCDpresent("TCDpres", "TCD connected by wire", settings.TCDpresent, 1, "autocomplete='off' title='Check if you have a Time Circuits Display connected via wire' type='checkbox' style='margin-top:5px;'", WFM_LABEL_AFTER);
#endif // -------------------------------------------------
#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
WiFiManagerParameter custom_noETTOL("uEtNL", "TCD signals Time Travel without 5s lead (0=no, 1=yes)", settings.noETTOLead, 1, "autocomplete='off'");
#else // -------------------- Checkbox hack: --------------
WiFiManagerParameter custom_noETTOL("uEtNL", "TCD signals Time Travel without 5s lead", settings.noETTOLead, 1, "autocomplete='off' type='checkbox' class='mt5' style='margin-left:20px'", WFM_LABEL_AFTER);
#endif // -------------------------------------------------

#ifdef BTTFN_MC
WiFiManagerParameter custom_tcdIP("tcdIP", "IP address or hostname of TCD", settings.tcdIP, 63, "pattern='(^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}$)|([A-Za-z0-9\\-]+)' placeholder='Example: 192.168.4.1'");
#else
WiFiManagerParameter custom_tcdIP("tcdIP", "IP address of TCD", settings.tcdIP, 63, "pattern='^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}$' placeholder='Example: 192.168.4.1'");
#endif
#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
WiFiManagerParameter custom_uGPS("uGPS", "Adapt pattern to TCD-provided speed (0=no, 1=yes)<br><span style='font-size:80%'>Speed from TCD (GPS, rotary encoder, remote control), if available, will overrule idle pattern</span>", settings.useGPSS, 1, "autocomplete='off'");
#else // -------------------- Checkbox hack: --------------
WiFiManagerParameter custom_uGPS("uGPS", "Adapt pattern to TCD-provided speed<br><span style='font-size:80%'>Speed from TCD (GPS, rotary encoder, remote control), if available, will overrule idle pattern</span>", settings.useGPSS, 1, "autocomplete='off' type='checkbox' style='margin-bottom:0px;'", WFM_LABEL_AFTER);
#endif // -------------------------------------------------
#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
WiFiManagerParameter custom_uNM("uNM", "Follow TCD night-mode (0=no, 1=yes)<br><span style='font-size:80%'>If enabled, the Screen Saver will activate when TCD is in night-mode.</span>", settings.useNM, 1, "autocomplete='off'");
#else // -------------------- Checkbox hack: --------------
WiFiManagerParameter custom_uNM("uNM", "Follow TCD night-mode<br><span style='font-size:80%'>If checked, the Screen Saver will activate when TCD is in night-mode.</span>", settings.useNM, 1, "autocomplete='off' type='checkbox' style='margin-bottom:0px;'", WFM_LABEL_AFTER);
#endif // -------------------------------------------------
#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
WiFiManagerParameter custom_uFPO("uFPO", "Follow TCD fake power (0=no, 1=yes)", settings.useFPO, 1, "autocomplete='off'");
#else // -------------------- Checkbox hack: --------------
WiFiManagerParameter custom_uFPO("uFPO", "Follow TCD fake power", settings.useFPO, 1, "autocomplete='off' type='checkbox' style='margin-bottom:0px;'", WFM_LABEL_AFTER);
#endif // -------------------------------------------------
#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
WiFiManagerParameter custom_bttfnTT("bttfnTT", "'0' and button trigger BTTFN-wide TT (0=no, 1=yes)<br><span style='font-size:80%'>If enabled, pressing '0' on the IR remote or pressing the Time Travel button triggers a BTTFN-wide TT</span>", settings.bttfnTT, 1, "autocomplete='off'");
#else // -------------------- Checkbox hack: --------------
WiFiManagerParameter custom_bttfnTT("bttfnTT", "'0' and button trigger BTTFN-wide TT<br><span style='font-size:80%'>If checked, pressing '0' on the IR remote or pressing the Time Travel button triggers a BTTFN-wide TT</span>", settings.bttfnTT, 1, "autocomplete='off' type='checkbox' style='margin-bottom:0px;'", WFM_LABEL_AFTER);
#endif // -------------------------------------------------
#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
WiFiManagerParameter custom_ssClock("ssClk", "Show clock when Screen Saver is active (0=no, 1=yes)", settings.ssClock, 1, "autocomplete='off' ");
#else // -------------------- Checkbox hack: --------------
WiFiManagerParameter custom_ssClock("ssClk", "Show clock when Screen Saver is active", settings.ssClock, 1, "type='checkbox' style='margin-bottom:0px;'", WFM_LABEL_AFTER);
#endif // -------------------------------------------------
#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
WiFiManagerParameter custom_ssClockO("ssClkO", "Clock off in Night Mode (0=no, 1=yes)", settings.ssClockOffNM, 1, "autocomplete='off' ");
#else // -------------------- Checkbox hack: --------------
WiFiManagerParameter custom_ssClockO("ssClkO", "Clock off in Night Mode", settings.ssClockOffNM, 1, "type='checkbox' style='margin-bottom:0px;margin-left:20px'", WFM_LABEL_AFTER);
#endif // -------------------------------------------------

#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
WiFiManagerParameter custom_sStrict("sStrict", "Adhere strictly to movie patterns (0=no, 1=yes)<br><span style='font-size:80%'>Check to strictly show movie patterns in idle modes 0-3 and with GPS speed; uncheck to allow variations.</span>", settings.strictMode, 1, "autocomplete='off'");
#else // -------------------- Checkbox hack: --------------
WiFiManagerParameter custom_sStrict("sStrict", "Adhere strictly to movie patterns<br><span style='font-size:80%'>Check to strictly show movie patterns in idle modes 0-3 and with GPS speed; uncheck to allow variations.</span>", settings.strictMode, 1, "autocomplete='off' type='checkbox' style='margin-top:5px;'", WFM_LABEL_AFTER);
#endif // -------------------------------------------------
#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
WiFiManagerParameter custom_sTTANI("sTTANI", "Skip time tunnel animation (0=no, 1=yes)", settings.skipTTAnim, 1, "autocomplete='off' title='Enable to skip the time tunnel animation'");
#else // -------------------- Checkbox hack: --------------
WiFiManagerParameter custom_sTTANI("sTTANI", "Skip time tunnel animation", settings.skipTTAnim, 1, "autocomplete='off' title='Check to skip the time tunnel animation' type='checkbox'", WFM_LABEL_AFTER);
#endif // -------------------------------------------------
#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
WiFiManagerParameter custom_SApeaks("sap", "Show peaks in Spectrum Analyzer (0=off, 1=on)", settings.SApeaks, 1, "autocomplete='off'");
#else // -------------------- Checkbox hack: --------------
WiFiManagerParameter custom_SApeaks("sap", "Show peaks in Spectrum Analyzer", settings.SApeaks, 1, "type='checkbox'", WFM_LABEL_AFTER);
#endif // -------------------------------------------------

#ifdef SID_HAVEMQTT
#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
WiFiManagerParameter custom_useMQTT("uMQTT", "Use Home Assistant (0=no, 1=yes)", settings.useMQTT, 1, "autocomplete='off'");
#else // -------------------- Checkbox hack: --------------
WiFiManagerParameter custom_useMQTT("uMQTT", "Use Home Assistant (MQTT 3.1.1)", settings.useMQTT, 1, "type='checkbox' style='margin-top:5px'", WFM_LABEL_AFTER);
#endif // -------------------------------------------------
WiFiManagerParameter custom_mqttServer("ha_server", "<br>Broker IP[:port] or domain[:port]", settings.mqttServer, 79, "pattern='[a-zA-Z0-9\\.:\\-]+' placeholder='Example: 192.168.1.5'");
WiFiManagerParameter custom_mqttUser("ha_usr", "User[:Password]", settings.mqttUser, 63, "placeholder='Example: ronald:mySecret'");
#endif // HAVEMQTT

#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
WiFiManagerParameter custom_CfgOnSD("CfgOnSD", "Save secondary settings on SD (0=no, 1=yes)<br><span style='font-size:80%'>Enable this to avoid flash wear</span>", settings.CfgOnSD, 1, "autocomplete='off'");
#else // -------------------- Checkbox hack: --------------
WiFiManagerParameter custom_CfgOnSD("CfgOnSD", "Save secondary settings on SD<br><span style='font-size:80%'>Check this to avoid flash wear</span>", settings.CfgOnSD, 1, "autocomplete='off' type='checkbox' style='margin-top:5px'", WFM_LABEL_AFTER);
#endif // -------------------------------------------------
//#ifdef TC_NOCHECKBOXES  // --- Standard text boxes: -------
//WiFiManagerParameter custom_sdFrq("sdFrq", "SD clock speed (0=16Mhz, 1=4Mhz)<br><span style='font-size:80%'>Slower access might help in case of problems with SD cards</span>", settings.sdFreq, 1, "autocomplete='off'");
//#else // -------------------- Checkbox hack: --------------
//WiFiManagerParameter custom_sdFrq("sdFrq", "4MHz SD clock speed<br><span style='font-size:80%'>Checking this might help in case of SD card problems</span>", settings.sdFreq, 1, "autocomplete='off' type='checkbox' style='margin-top:12px'", WFM_LABEL_AFTER);
//#endif // -------------------------------------------------

WiFiManagerParameter custom_sectstart_head("<div class='sects'>");
WiFiManagerParameter custom_sectstart("</div><div class='sects'>");
WiFiManagerParameter custom_sectend("</div>");

WiFiManagerParameter custom_sectstart_nw("</div><div class='sects'><div class='headl'>Wireless communication (BTTF-Network)</div>");

WiFiManagerParameter custom_sectend_foot("</div><p></p>");

#define TC_MENUSIZE 6
static const char* wifiMenu[TC_MENUSIZE] = {"wifi", "param", "sep", "update", "sep", "custom" };

static const char* myHead = "<link rel='shortcut icon' type='image/png' href=' data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAMAAAAoLQ9TAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAA9QTFRFjpCRzMvHQ7Yk9tgx8iU9dfM6hQAAADJJREFUeNpiYEQDDIwMKAAhwMKCJsDMTIQASAwkAJEjIABHYAEmJgSimgC659AAQIABAHNsAOmY7Q19AAAAAElFTkSuQmCC'><script>function wlp(){return window.location.pathname;}function getn(x){return document.getElementsByTagName(x)}function ge(x){return document.getElementById(x)}function c(l){ge('s').value=l.getAttribute('data-ssid')||l.innerText||l.textContent;p=l.nextElementSibling.classList.contains('l');ge('p').disabled=!p;if(p){ge('p').placeholder='';ge('p').focus();}}window.onload=function(){xx=false;document.title='Status Indicator';if(ge('s')&&ge('dns')){xx=true;xxx=document.title;yyy='Configure WiFi';aa=ge('s').parentElement;bb=aa.innerHTML;dd=bb.search('<hr>');ee=bb.search('<button');cc='<div class=\"sects\">'+bb.substring(0,dd)+'</div><div class=\"sects\">'+bb.substring(dd+4,ee)+'</div>'+bb.substring(ee);aa.innerHTML=cc;document.querySelectorAll('a[href=\"#p\"]').forEach((userItem)=>{userItem.onclick=function(){c(this);return false;}});if(aa=ge('s')){aa.oninput=function(){if(this.placeholder.length>0&&this.value.length==0){ge('p').placeholder='********';}}}}if(ge('uploadbin')||wlp()=='/u'||wlp()=='/wifisave'||wlp()=='/paramsave'){xx=true;xxx=document.title;yyy=(wlp()=='/wifisave')?'Configure WiFi':(wlp()=='/paramsave'?'Setup':'Firmware update');aa=document.getElementsByClassName('wrap');if(aa.length>0){if((bb=ge('uploadbin'))){aa[0].style.textAlign='center';bb.parentElement.onsubmit=function(){aa=document.getElementById('uploadbin');if(aa){aa.disabled=true;aa.innerHTML='Please wait'}}}aa=getn('H3');if(aa.length>0){aa[0].remove()}aa=getn('H1');if(aa.length>0){aa[0].remove()}}}if(ge('ttrp')||wlp()=='/param'){xx=true;xxx=document.title;yyy='Setup';}if(ge('ebnew')){xx=true;bb=getn('H3');aa=getn('H1');xxx=aa[0].innerHTML;yyy=bb[0].innerHTML;ff=aa[0].parentNode;ff.style.position='relative';}if(xx){zz=(Math.random()>0.8);dd=document.createElement('div');dd.classList.add('tpm0');dd.innerHTML='<div class=\"tpm\" onClick=\"window.location=\\'/\\'\"><div class=\"tpm2\"><img src=\"data:image/png;base64,'+(zz?'iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAMAAACdt4HsAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAAZQTFRFSp1tAAAA635cugAAAAJ0Uk5T/wDltzBKAAAAbUlEQVR42tzXwRGAQAwDMdF/09QQQ24MLkDj77oeTiPA1wFGQiHATOgDGAp1AFOhDWAslAHMhS6AQKgCSIQmgEgoAsiEHoBQqAFIhRaAWCgByIVXAMuAdcA6YBlwALAKePzgd71QAByP71uAAQC+xwvdcFg7UwAAAABJRU5ErkJggg==':'iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAMAAACdt4HsAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAAZQTFRFSp1tAAAA635cugAAAAJ0Uk5T/wDltzBKAAAAgElEQVR42tzXQQqDABAEwcr/P50P2BBUdMhee6j7+lw8i4BCD8MiQAjHYRAghAh7ADWMMAcQww5jADHMsAYQwwxrADHMsAYQwwxrADHMsAYQwwxrgLgOPwKeAjgrrACcFkYAzgu3AN4C3AV4D3AP4E3AHcDF+8d/YQB4/Pn+CjAAMaIIJuYVQ04AAAAASUVORK5CYII=')+'\" class=\"tpm3\"></div><H1 class=\"tpmh1\"'+(zz?' style=\"margin-left:1.2em\"':'')+'>'+xxx+'</H1>'+'<H3 class=\"tpmh3\"'+(zz?' style=\"padding-left:4.5em\"':'')+'>'+yyy+'</div></div>';}if(ge('ebnew')){bb[0].remove();aa[0].replaceWith(dd);}if((ge('s')&&ge('dns'))||ge('uploadbin')||wlp()=='/u'||wlp()=='/wifisave'||wlp()=='/paramsave'||ge('ttrp')||wlp()=='/param'){aa=document.getElementsByClassName('wrap');if(aa.length>0){aa[0].insertBefore(dd,aa[0].firstChild);aa[0].style.position='relative';}}}</script><style type='text/css'>body{font-family:-apple-system,BlinkMacSystemFont,system-ui,'Segoe UI',Roboto,'Helvetica Neue',Verdana,Helvetica}H1,H2{margin-top:0px;margin-bottom:0px;text-align:center;}H3{margin-top:0px;margin-bottom:5px;text-align:center;}div.msg{border:1px solid #ccc;border-left-width:15px;border-radius:20px;background:linear-gradient(320deg,rgb(255,255,255) 0%,rgb(235,234,233) 100%);}button{transition-delay:250ms;margin-top:10px;margin-bottom:10px;color:#fff;background-color:#225a98;font-variant-caps:all-small-caps;}button.DD{color:#000;border:4px ridge #999;border-radius:2px;background:#e0c942;background-image:url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAADBQTFRF////AAAAMyks8+AAuJYi3NHJo5aQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAbP19EwAAAAh0Uk5T/////////wDeg71ZAAAA4ElEQVR42qSTyxLDIAhF7yChS/7/bwtoFLRNF2UmRr0H8IF4/TBsY6JnQFvTJ8D0ncChb0QGlDvA+hkw/yC4xED2Z2L35xwDRSdqLZpFIOU3gM2ox6mA3tnDPa8UZf02v3q6gKRH/Eyg6JZBqRUCRW++yFYIvCjNFIt9OSC4hol/ItH1FkKRQgAbi0ty9f/F7LM6FimQacPbAdG5zZVlWdfvg+oEpl0Y+jzqIJZ++6fLqlmmnq7biZ4o67lgjBhA0kvJyTww/VK0hJr/LHvBru8PR7Dpx9MT0f8e72lvAQYALlAX+Kfw0REAAAAASUVORK5CYII=');background-repeat:no-repeat;background-origin:content-box;background-size:contain;}br{display:block;font-size:1px;content:''}input[type='checkbox']{display:inline-block;margin-top:10px}input{border:thin inset}small{display:none}em > small{display:inline}form{margin-block-end:0;}.tpm{cursor:pointer;border:1px solid black;border-radius:5px;padding:0 0 0 0px;min-width:18em;}.tpm2{position:absolute;top:-0.7em;z-index:130;left:0.7em;}.tpm3{width:4em;height:4em;}.tpmh1{font-variant-caps:all-small-caps;font-weight:normal;overflow:clip;font-size:1.7em;margin:0.1em 0em 0.1em 2.3em;}.tpmh3{background:#000;font-size:0.6em;color:#ffa;padding-left:7em;margin-left:0.5em;margin-right:0.5em;border-radius:5px}.sects{background-color:#eee;border-radius:7px;margin-bottom:20px;padding-bottom:7px;padding-top:7px}.tpm0{position:relative;width:20em;margin:0 auto 0 auto;}.headl{margin:0 0 5px 0;padding:0}.cmp0{margin:0;padding:0;}.sel0{font-size:90%;width:auto;margin-left:10px;vertical-align:baseline;}.mt5{margin-top:5px!important}</style>";
static const char* myCustMenu = "<form action='/erase' method='get' onsubmit='return confirm(\"This erases the WiFi config and reboots. The device will restart in access point mode. Are you sure?\");'><button id='ebnew' class='DD'>Erase WiFi Config</button></form><br/><img style='display:block;margin:10px auto 10px auto;' src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAR8AAAAyCAYAAABlEt8RAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAADQ9JREFUeNrsXTFzG7sRhjTuReYPiGF+gJhhetEzTG2moFsrjVw+vYrufOqoKnyl1Zhq7SJ0Lc342EsT6gdIof+AefwFCuksnlerBbAA7ygeH3bmRvTxgF3sLnY/LMDzjlKqsbgGiqcJXEPD97a22eJKoW2mVqMB8HJRK7D/1DKG5fhH8NdHrim0Gzl4VxbXyeLqLK4DuDcGvXF6P4KLG3OF8JtA36a2J/AMvc/xTh3f22Q00QnSa0r03hGOO/Wws5Y7RD6brbWPpJ66SNHl41sTaDMSzMkTxndriysBHe/BvVs0XyeCuaEsfqblODHwGMD8+GHEB8c1AcfmJrurbSYMHK7g8CC4QknS9zBQrtSgO22gzJNnQp5pWOyROtqa7k8cOkoc+kyEOm1ZbNAQyv7gcSUryJcG+kiyZt9qWcagIBhkjn5PPPWbMgHX1eZoVzg5DzwzDKY9aFtT5aY3gknH0aEF/QxRVpDyTBnkxH3WvGmw0zR32Pu57XVUUh8ZrNm3hh7PVwQ+p1F7KNWEOpjuenR6wEArnwCUqPJT6IQ4ZDLQEVpm2eg9CQQZY2wuuJicD0NlG3WeWdedkvrILxak61rihbR75bGyOBIEHt+lLDcOEY8XzM0xYt4i2fPEEdV+RUu0I1BMEc70skDnuUVBtgWTX9M+GHrikEuvqffJ+FOiS6r3AYLqB6TtwBA0ahbko8eQMs9OBY46KNhetgDo0rWp76/o8wVBBlOH30rloz5CJ1zHgkg0rw4EKpygTe0wP11Lob41EdiBzsEvyMZ6HFNlrtFeGOTLLAnwC/hzBfGYmNaICWMAaY2h5WgbCuXTnGo7kppPyhT+pHUAGhRM/dYcNRbX95mhXpB61FUSQV2illPNJ7TulgT0KZEzcfitywdTZlJL5W5Z2g2E/BoW32p5+GuN8bvOCrU+zo4VhscPmSTLrgGTSaU0smTpslAoBLUhixZT+6Ftb8mS15SRJciH031IpoxLLxmCqwXOj0YgvxCaMz46Ve7dWd9VRMbwSKXBZxKooEhmkgSC1BKwpoaAc+DB0wStv+VQ48qLNqHwHZJoKiWQea+guTyX2i8k+Pg4Q8UDDWwqdQrIOjWBXjKhsx8wur5gkkVFiOj2Eep6rsn/pWTop1aAjxRBGYO48w5AEymPF2ucuPMcg08ivBfqSAnK/LiwN1byA5Mt4VLJFHxsQX/CBPmGAxn5OFmKglpL+W3nSu01tPjDlKCvQcF+emRYCk8DbS1tV8lhXvmUBpbPvSKJ6z+L6xR0nAnGmTBjHRIeeJPqEPFIQoLPNzIJXUasgIL2LevbVeh9gcFn39D/rSALJyhQvHGs732zVM3yXYM48hTZjAs6YwfvpTP9ghx9WIC9UsskzUDfB2tCX2885cMJqqWenqdKcw4itZx8a6D4Ix7v4f6Jo69DZqxj4h8DJmljHr/vzEmDzxR1VvE0okY9iSovzUFxWcAk08uINEd5uL4o8tE222Oys2scExS8Xj1TDWPp0P/a0KXXvsXWpw7k00D2OBEu12z8LjyXeXry7zE8hiDXKstG/dOY1MAjBR2IDxlWPByXQ02tktZ7NOlT2kcBbS9UMYXbOYHD9ADhxBCYpDWJ0TPXXUYEUZeBTgVJdhlQv0Iw2SPzxBcd/xagmyn4wxeDnw9z0MMEeIwNPEY+yOdgBUFSlX8BrshDhmOydEwQgvjogOOmDJ7lIFfGGPjQEGAy8nyFPDsVyo2XXmMGcq9ir4lgkuClV5FFXO6QYQi/VSZuyK8HQksZU7BpC2TeJ3O9Y+ibO2SYWXi00LJ9j/Bo7BZgxJck4r0pALanzJU3ZernL6CVMAsvx/4Pj+eVZSnbckyGzIB8bpnnG4xjSLKX3nZfdenF2SvznMxFHvGYeMp3C7b+1VHDkSLYfzoCye0KvuWyS0M9PlNm0/WU0ZMrSC/HVWN4tHYDJkYmMOIwB6NsCqVCw+hnR0TRXPD16dOmaw6dZobgFJLVRzmh3zx0f7BBPqFfFzMgy19JMLiA5dkpBJOaADFlBt/q5DSWZA36ojuWFUnwCXHc0RYFHwlKccHvjiOA15g+XHWaqUGmlJm4Pgkkr2VEXojk24b7Aw3QDYFOE7hGAUvyEamf5DG3pmvQ0xMekuATcqYgI0svCtv1j8z0Vct5oDXSf2XFvlZdi7t02GECHA763xR/TN2FCnRWxrWacckm/0htNo1yXgoVmdgrhrmQp8xiHruOThL1ePt87lFfsRllmR2+oitvgx2R/kPrBR0GLkrGPyXwmAbfCYHrr9TPX/5qGL7n4DkRLFUmWzD5hyUIPvM1onyaEDqe82IKfyvoXidHJITfjqksPFIu+Cy3AJe/Rp2pp2cLRis4bZ4BRvLmuVA6RP39Wz0+EepjGNfSa8jofanz/zI8BwZ0GQKnU099pAXaKwmYbEXQ1xXkozraV8X//jF06dVSP3dtZzDGj+rpgUDTPH+v3G8RbUF/H9F3H0kynZuCj7JAeJ/tQJr9y/IjQZcORoGTljpIouxvE9T0xYJgxg6+08CgZcvscen1/EuvYSA/SXL+Ta12NERyHGMgrfnoSdcKEMqV/ctGRx46oBmbLr0ygdPcOp7JDDUeW/CZlHDyl2HptU4/d/kWRw3lfsPgrVpt50sS3PTLxZzBZynMhZK9UW4TjFIEjUEHfw6YhK7xL7//q3p62nQOPF0B33Uwbipcim168Nn0Xa+M2HDdSy/J3Frq8CX41Zzxt9NAgEFRt4nHN+CxTTvfW0WNLViaRioH1VQxO81iHjsPDw/RDJEiRVo77UYVRIoUKQafSJEixeATKVKkSDH4RIoUKQafSJEiRYrBJ1KkSDH4RIoUKVIMPpEiRYrBJ1KkSJFi8IkUKVIMPpEiRYrBJ1KkSJFi8IkUKdIfg15s02B2dnaWf+qLq7u4qur/r4r8vLjuDU168PfM0fUx9Ef7ou17TNurxXUTMJwq4jtDY5kxz2hafncOn9uLqwm8r9C/OaLynxM+PdS3lomjG9BPFz2v7SF9ntO7MsjlIuoL96BDZRmHloPTF7YB1v2ZxV/qxA5UNqyLK6FsmE8d6eSHf5bmTRVLQbflAkNw75ftGgIPff+siS7huTZVH2lver/tB0+zLMfxnennGj3TNDxzR8bXY8Zrev/uA2mD718SXXBXD3SEn297Pq+D6jXz/HdLAKXUNfDsO8Zx6dAXluEO7tUJb32/ythBBw2bn7hkUwb9/OBZlvm6VcgHMpvOIFdg5C78/Uycu4cyWN70jvA5hux4L2yPM+c5fG6TrP8J7t+gsXUFKOuKZGCO+hbE+Bm178Mz5yh722xzziAfE/8mjPcMBdumB4rsIVvcIKRB25+Tcc4s+uqCDEv7vAVd9OA+lrMObWaGxPIB6fIGySuVrYt0cQb320hnEfk8A/JRTDDR2UqRiXuNslLeyEfSNoRfFTm4Rjl0vE0H8unZ3AGhqU8G5KMc903I59LAk/tey9A0jE3k2gbbVoV24fRFZe0yunLpvce00XLVV5Dt97FF5PN8NCNZhmbYNjjN3zwDgq/zr0I3INsnyGy6bjRDYzDVQFzIoE7GfU+yq67DHMNzVzmNqUr4zgyytuFZrlZ246nDJiSZc+jvntFXk2knRQ+fiT1wf1eWYKsYFDjzkO0eIcQqQmezUs3ULUQ+FOE8oMJgFdBCn2QQKRLxqZn0AF7TWo10ot4x6/2qB4qR1nx6DPLRNafrHJGPqX7hi5Sk1GZqYn2BTdtEX5fInndMDfETQWnfUd2Ns4MECbtkw3xxra8Zkc9mkF6Ln6MsI93dMhFdg/ctNQucHd8GoLe/QNBswjjaEMxer6gXWvO5YQLfPeiorx7vpq2KSG8CUUzoOKkOe6SOxNn0nglibTSG16R+eIPsU0W1ujzIJttrJFsXEsYyaP0pIp/nRT7HaF1dJZn6Dox0iTKZK8v61nzaJHOuSnXC61i5d9FCaz4PBH3drbnmU1ePd+3yomPF79q56iof4Jk7w/N1gpAoMqJ6/0DQuI+/2ZCy3v1ql2W+buMhw2Mw8Dlkh5mh5tFGNaF2zjJcQXbVtZtj4ow99XR7FlPXINOM1BOOSd/tnJHKmUPOIkjXoOokuNYdgZMLHnVHTVAqz1Lf71Dw4OTFCOnKUYvS6LhJ5JXWFKku8K5t3O16RuTjqstw2U1a8/Hd7WozWfxBkNWuCUr7ztQs+urx2ZPvSnbOByM/fTUN8uOxr3O3q8vUM/RnSTCsqsdno3ANpUvGdc3ow4QULw2opa/4szimfq4NY/sglK2P7I4R/HWs+USi9RW9DJPWms5RraKO6lS4/TvIcj2U9e4FPOrMBLaddTorABm66DOg1j6SVyMxaWZ/h3SIkRytx/jsYGpd6HNQM6Z+Jdkd/Duqp9VRO6lsV+rnuSWMtt6WaXJs1X8aCD+v2DaqK/nhxEh/PB0+GVtZ5vT/BBgARwZUDnOS4TkAAAAASUVORK5CYII='><div style='font-size:10px;margin-left:auto;margin-right:auto;text-align:center;'>Version " SID_VERSION " (" SID_VERSION_EXTRA ")<br>Powered by <a href='https://sid.out-a-ti.me'>A10001986 [Documentation]</a></div>";

static int  shouldSaveConfig = 0;
static bool shouldSaveIPConfig = false;
static bool shouldDeleteIPConfig = false;

// WiFi power management in AP mode
bool          wifiInAPMode = false;
bool          wifiAPIsOff = false;
unsigned long wifiAPModeNow;
unsigned long wifiAPOffDelay = 0;     // default: never

// WiFi power management in STA mode
bool          wifiIsOff = false;
unsigned long wifiOnNow = 0;
unsigned long wifiOffDelay     = 0;   // default: never
unsigned long origWiFiOffDelay = 0;

#ifdef SID_HAVEMQTT
#define       MQTT_SHORT_INT  (30*1000)
#define       MQTT_LONG_INT   (5*60*1000)
bool          useMQTT = false;
char          mqttUser[64] = { 0 };
char          mqttPass[64] = { 0 };
char          mqttServer[80] = { 0 };
uint16_t      mqttPort = 1883;
bool          pubMQTT = false;
static unsigned long mqttReconnectNow = 0;
static unsigned long mqttReconnectInt = MQTT_SHORT_INT;
static uint16_t      mqttReconnFails = 0;
static bool          mqttSubAttempted = false;
static bool          mqttOldState = true;
static bool          mqttDoPing = true;
static bool          mqttRestartPing = false;
static bool          mqttPingDone = false;
static unsigned long mqttPingNow = 0;
static unsigned long mqttPingInt = MQTT_SHORT_INT;
static uint16_t      mqttPingsExpired = 0;
#endif

static void wifiConnect(bool deferConfigPortal = false);
static void saveParamsCallback();
static void saveConfigCallback();
static void preUpdateCallback();
static void preSaveConfigCallback();
static void waitConnectCallback();

static void setupStaticIP();
static void ipToString(char *str, IPAddress ip);
static IPAddress stringToIp(char *str);

static void getParam(String name, char *destBuf, size_t length);
static bool myisspace(char mychar);
static char* strcpytrim(char* destination, const char* source, bool doFilter = false);
static void mystrcpy(char *sv, WiFiManagerParameter *el);
#ifndef TC_NOCHECKBOXES
static void strcpyCB(char *sv, WiFiManagerParameter *el);
static void setCBVal(WiFiManagerParameter *el, char *sv);
#endif

#ifdef SID_HAVEMQTT
static void strcpyutf8(char *dst, const char *src, unsigned int len);
static void mqttPing();
static bool mqttReconnect(bool force = false);
static void mqttLooper();
static void mqttCallback(char *topic, byte *payload, unsigned int length);
static void mqttSubscribe();
#endif

/*
 * wifi_setup()
 *
 */
void wifi_setup()
{
    int temp, wifiretry;

    // Explicitly set mode, esp allegedly defaults to STA_AP
    WiFi.mode(WIFI_MODE_STA);

    #ifndef SID_DBG
    wm.setDebugOutput(false);
    #endif

    wm.setParamsPage(true);
    wm.setBreakAfterConfig(true);
    wm.setConfigPortalBlocking(false);
    wm.setPreSaveConfigCallback(preSaveConfigCallback);
    wm.setSaveConfigCallback(saveConfigCallback);
    wm.setSaveParamsCallback(saveParamsCallback);
    wm.setPreOtaUpdateCallback(preUpdateCallback);
    wm.setHostname(settings.hostName);
    wm.setCaptivePortalEnable(false);
    
    // Our style-overrides, the page title
    wm.setCustomHeadElement(myHead);
    wm.setTitle(F("Status Indicator"));
    wm.setDarkMode(false);

    // Hack version number into WiFiManager main page
    wm.setCustomMenuHTML(myCustMenu);

    // Static IP info is not saved by WiFiManager,
    // have to do this "manually". Hence ipsettings.
    wm.setShowStaticFields(true);
    wm.setShowDnsFields(true);

    temp = atoi(settings.wifiConTimeout);
    if(temp < 7) temp = 7;
    if(temp > 25) temp = 25;
    wm.setConnectTimeout(temp);

    wifiretry = atoi(settings.wifiConRetries);
    if(wifiretry < 1) wifiretry = 1;
    if(wifiretry > 10) wifiretry = 10;
    wm.setConnectRetries(wifiretry);

    wm.setCleanConnect(true);
    //wm.setRemoveDuplicateAPs(false);

    #ifdef WIFIMANAGER_2_0_17
    wm._preloadwifiscan = false;
    wm._asyncScan = true;
    #endif

    wm.setMenu(wifiMenu, TC_MENUSIZE);

    wm.addParameter(&custom_sectstart_head);// 3
    wm.addParameter(&custom_bootSA);
    wm.addParameter(&custom_ssDelay);

    wm.addParameter(&custom_sectstart);     // 2
    wm.addParameter(&custom_disDIR);
    
    wm.addParameter(&custom_sectstart);     // 6
    wm.addParameter(&custom_hostName);
    wm.addParameter(&custom_sysID);
    wm.addParameter(&custom_appw);
    wm.addParameter(&custom_wifiConRetries);
    wm.addParameter(&custom_wifiConTimeout); 

    wm.addParameter(&custom_sectstart);     // 3
    wm.addParameter(&custom_TCDpresent);
    wm.addParameter(&custom_noETTOL);

    wm.addParameter(&custom_sectstart_nw);  // 6
    wm.addParameter(&custom_tcdIP);
    wm.addParameter(&custom_uGPS);
    wm.addParameter(&custom_uNM);
    wm.addParameter(&custom_uFPO);
    wm.addParameter(&custom_bttfnTT);
    wm.addParameter(&custom_ssClock);
    wm.addParameter(&custom_ssClockO);

    wm.addParameter(&custom_sectstart);     // 4
    wm.addParameter(&custom_sStrict);
    wm.addParameter(&custom_sTTANI);
    wm.addParameter(&custom_SApeaks);

    #ifdef SID_HAVEMQTT
    wm.addParameter(&custom_sectstart);     // 4
    wm.addParameter(&custom_useMQTT);
    wm.addParameter(&custom_mqttServer);
    wm.addParameter(&custom_mqttUser);
    #endif
    
    wm.addParameter(&custom_sectstart);     // 2 (3)
    wm.addParameter(&custom_CfgOnSD);
    //wm.addParameter(&custom_sdFrq);
    
    wm.addParameter(&custom_sectend_foot);  // 1

    updateConfigPortalValues();

    #ifdef SID_MDNS
    if(MDNS.begin(settings.hostName)) {
        MDNS.addService("http", "tcp", 80);
    }
    #endif

    // No WiFi powersave features here
    wifiOffDelay = 0;
    wifiAPOffDelay = 0;
    
    // Configure static IP
    if(loadIpSettings()) {
        setupStaticIP();
    }

    // Find out if we have a configured WiFi network to connect to.
    // If we detect "TCD-AP" as the SSID, we make sure that we retry
    // at least 2 times so we have a chance to catch the TCD's AP if 
    // both are powered up at the same time.
    {
        wifi_config_t conf;
        esp_wifi_get_config(WIFI_IF_STA, &conf);
        if((conf.sta.ssid[0] != 0)) {
            if(!strncmp("TCD-AP", (const char *)conf.sta.ssid, 6)) {
                if(wifiretry < 2) {
                    wm.setConnectRetries(2);
                }
            }
        } else {
            // No point in retry when we have no WiFi config'd
            wm.setConnectRetries(1);
        }
    }

    wifi_setup2();
}

void wifi_setup2()
{
    // Connect, but defer starting the CP
    wifiConnect(true);

#ifdef SID_HAVEMQTT
    useMQTT = (atoi(settings.useMQTT) > 0);
    
    if((!settings.mqttServer[0]) || // No server -> no MQTT
       (wifiInAPMode))              // WiFi in AP mode -> no MQTT
        useMQTT = false;  
    
    if(useMQTT) {

        bool mqttRes = false;
        char *t;
        int tt;

        // No WiFi power save if we're using MQTT
        origWiFiOffDelay = wifiOffDelay = 0;

        if((t = strchr(settings.mqttServer, ':'))) {
            strncpy(mqttServer, settings.mqttServer, t - settings.mqttServer);
            mqttServer[t - settings.mqttServer + 1] = 0;
            tt = atoi(t+1);
            if(tt > 0 && tt <= 65535) {
                mqttPort = tt;
            }
        } else {
            strcpy(mqttServer, settings.mqttServer);
        }

        if(isIp(mqttServer)) {
            mqttClient.setServer(stringToIp(mqttServer), mqttPort);
        } else {
            IPAddress remote_addr;
            if(WiFi.hostByName(mqttServer, remote_addr)) {
                mqttClient.setServer(remote_addr, mqttPort);
            } else {
                mqttClient.setServer(mqttServer, mqttPort);
                // Disable PING if we can't resolve domain
                mqttDoPing = false;
                Serial.printf("MQTT: Failed to resolve '%s'\n", mqttServer);
            }
        }
        
        mqttClient.setCallback(mqttCallback);
        mqttClient.setLooper(mqttLooper);

        if(settings.mqttUser[0] != 0) {
            if((t = strchr(settings.mqttUser, ':'))) {
                strncpy(mqttUser, settings.mqttUser, t - settings.mqttUser);
                mqttUser[t - settings.mqttUser + 1] = 0;
                strcpy(mqttPass, t + 1);
            } else {
                strcpy(mqttUser, settings.mqttUser);
            }
        }

        #ifdef SID_DBG
        Serial.printf("MQTT: server '%s' port %d user '%s' pass '%s'\n", mqttServer, mqttPort, mqttUser, mqttPass);
        #endif
            
        mqttReconnect(true);
        // Rest done in loop
            
    } else {

        #ifdef SID_DBG
        Serial.println("MQTT: Disabled");
        #endif

    }
#endif

    // Start the Config Portal. A WiFiScan does not
    // disturb anything at this point hopefully.
    if(WiFi.status() == WL_CONNECTED) {
        wifiStartCP();
    }

    wifiSetupDone = true;
}

/*
 * wifi_loop()
 *
 */
void wifi_loop()
{
    char oldCfgOnSD = 0;

    if(!wifiSetupDone)
        return;

#ifdef SID_HAVEMQTT
    if(useMQTT) {
        if(mqttClient.state() != MQTT_CONNECTING) {
            if(!mqttClient.connected()) {
                if(mqttOldState || mqttRestartPing) {
                    // Disconnection first detected:
                    mqttPingDone = mqttDoPing ? false : true;
                    mqttPingNow = mqttRestartPing ? millis() : 0;
                    mqttOldState = false;
                    mqttRestartPing = false;
                    mqttSubAttempted = false;
                }
                if(mqttDoPing && !mqttPingDone) {
                    //audio_loop();
                    mqttPing();
                    //audio_loop();
                }
                if(mqttPingDone) {
                    //audio_loop();
                    mqttReconnect();
                    //audio_loop();
                }
            } else {
                // Only call Subscribe() if connected
                mqttSubscribe();
                mqttOldState = true;
            }
        }
        mqttClient.loop();
    }
#endif    
    
    wm.process();
    
    if(shouldSaveIPConfig) {

        #ifdef SID_DBG
        Serial.println(F("WiFi: Saving IP config"));
        #endif

        writeIpSettings();

        shouldSaveIPConfig = false;

    } else if(shouldDeleteIPConfig) {

        #ifdef SID_DBG
        Serial.println(F("WiFi: Deleting IP config"));
        #endif

        deleteIpSettings();

        shouldDeleteIPConfig = false;

    }

    if(shouldSaveConfig) {

        // Save settings and restart esp32

        #ifdef SID_DBG
        Serial.println(F("Config Portal: Saving config"));
        #endif

        // Only read parms if the user actually clicked SAVE on the params page
        if(shouldSaveConfig > 1) {

            int temp;

            // Save "strict" setting
            #ifdef TC_NOCHECKBOXES // --------- Plain text boxes:
            mystrcpy(settings.strictMode, &custom_sStrict);
            #else
            strcpyCB(settings.strictMode, &custom_sStrict);
            #endif
            if(settings.strictMode[0] == '1') {
                strictMode = true;
            } else if(settings.strictMode[0] == '0') {
                strictMode = false;
            }
            saveIdlePat();

            mystrcpy(settings.ssTimer, &custom_ssDelay);

            strcpytrim(settings.hostName, custom_hostName.getValue(), true);
            if(strlen(settings.hostName) == 0) {
                strcpy(settings.hostName, DEF_HOSTNAME);
            } else {
                char *s = settings.hostName;
                for( ; *s; ++s) *s = tolower(*s);
            }
            strcpytrim(settings.systemID, custom_sysID.getValue(), true);
            strcpytrim(settings.appw, custom_appw.getValue(), true);
            if((temp = strlen(settings.appw)) > 0) {
                if(temp < 8) {
                    settings.appw[0] = 0;
                }
            }
            mystrcpy(settings.wifiConRetries, &custom_wifiConRetries);
            mystrcpy(settings.wifiConTimeout, &custom_wifiConTimeout);

            strcpytrim(settings.tcdIP, custom_tcdIP.getValue());
            if(strlen(settings.tcdIP) > 0) {
                char *s = settings.tcdIP;
                for( ; *s; ++s) *s = tolower(*s);
            }

            #ifdef SID_HAVEMQTT
            strcpytrim(settings.mqttServer, custom_mqttServer.getValue());
            strcpyutf8(settings.mqttUser, custom_mqttUser.getValue(), sizeof(settings.mqttUser));
            #endif
            
            #ifdef TC_NOCHECKBOXES // --------- Plain text boxes:

            mystrcpy(settings.bootSA, &custom_bootSA);

            mystrcpy(settings.disDIR, &custom_disDIR);

            mystrcpy(settings.TCDpresent, &custom_TCDpresent);
            mystrcpy(settings.noETTOLead, &custom_noETTOL);

            mystrcpy(settings.useGPSS, &custom_uGPS);
            mystrcpy(settings.useNM, &custom_uNM);
            mystrcpy(settings.useFPO, &custom_uFPO);
            mystrcpy(settings.bttfnTT, &custom_bttfnTT);
            mystrcpy(settings.ssClock, &custom_ssClock);
            mystrcpy(settings.ssClockOffNM, &custom_ssClockO);

            mystrcpy(settings.skipTTAnim, &custom_sTTANI);
            mystrcpy(settings.SApeaks, &custom_SApeaks);

            #ifdef SID_HAVEMQTT
            mystrcpy(settings.useMQTT, &custom_useMQTT);
            #endif

            oldCfgOnSD = settings.CfgOnSD[0];
            mystrcpy(settings.CfgOnSD, &custom_CfgOnSD);
            //mystrcpy(settings.sdFreq, &custom_sdFrq);

            #else // -------------------------- Checkboxes:

            strcpyCB(settings.bootSA, &custom_bootSA);

            strcpyCB(settings.disDIR, &custom_disDIR);

            strcpyCB(settings.TCDpresent, &custom_TCDpresent);
            strcpyCB(settings.noETTOLead, &custom_noETTOL);

            strcpyCB(settings.useGPSS, &custom_uGPS);
            strcpyCB(settings.useNM, &custom_uNM);
            strcpyCB(settings.useFPO, &custom_uFPO);
            strcpyCB(settings.bttfnTT, &custom_bttfnTT);
            strcpyCB(settings.ssClock, &custom_ssClock);
            strcpyCB(settings.ssClockOffNM, &custom_ssClockO);

            strcpyCB(settings.skipTTAnim, &custom_sTTANI);
            strcpyCB(settings.SApeaks, &custom_SApeaks);

            #ifdef SID_HAVEMQTT
            strcpyCB(settings.useMQTT, &custom_useMQTT);
            #endif

            oldCfgOnSD = settings.CfgOnSD[0];
            strcpyCB(settings.CfgOnSD, &custom_CfgOnSD);
            //strcpyCB(settings.sdFreq, &custom_sdFrq);

            #endif  // -------------------------

            // Copy volume/speed/IR settings to other medium if
            // user changed respective option
            if(oldCfgOnSD != settings.CfgOnSD[0]) {
                copySettings();
            }

        }

        // Write settings if requested, or no settings file exists
        if(shouldSaveConfig > 1 || !checkConfigExists()) {
            write_settings();
        }

        
        shouldSaveConfig = 0;

        // Reset esp32 to load new settings

        allOff();

        flushDelayedSave();

        unmount_fs();

        #ifdef SID_DBG
        Serial.println(F("Config Portal: Restarting ESP...."));
        #endif

        Serial.flush();

        delay(1000);

        esp_restart();
    }

    // WiFi power management
    // If a delay > 0 is configured, WiFi is powered-down after timer has
    // run out. The timer starts when the device is powered-up/boots.
    // There are separate delays for AP mode and STA mode.
    // WiFi can be re-enabled for the configured time by holding '7'
    // on the keypad.
    // NTP requests will re-enable WiFi (in STA mode) for a short
    // while automatically.
    if(wifiInAPMode) {
        // Disable WiFi in AP mode after a configurable delay (if > 0)
        if(wifiAPOffDelay > 0) {
            if(!wifiAPIsOff && (millis() - wifiAPModeNow >= wifiAPOffDelay)) {
                wifiOff();
                wifiAPIsOff = true;
                wifiIsOff = false;
                #ifdef SID_DBG
                Serial.println(F("WiFi (AP-mode) is off. Hold '7' to re-enable."));
                #endif
            }
        }
    } else {
        // Disable WiFi in STA mode after a configurable delay (if > 0)
        if(origWiFiOffDelay > 0) {
            if(!wifiIsOff && (millis() - wifiOnNow >= wifiOffDelay)) {
                wifiOff();
                wifiIsOff = true;
                wifiAPIsOff = false;
                #ifdef SID_DBG
                Serial.println(F("WiFi (STA-mode) is off. Hold '7' to re-enable."));
                #endif
            }
        }
    }

}

static void wifiConnect(bool deferConfigPortal)
{
    const char *apName = "SID-AP";
    char realAPName[16];

    strcpy(realAPName, apName);
    if(settings.systemID[0]) {
        strcat(realAPName, settings.systemID);
    }
    
    // Automatically connect using saved credentials if they exist
    // If connection fails it starts an access point with the specified name
    if(wm.autoConnect(realAPName, settings.appw)) {
        #ifdef SID_DBG
        Serial.println(F("WiFi connected"));
        #endif

        // Since WM 2.0.13beta, starting the CP invokes an async
        // WiFi scan. This interferes with network access for a 
        // few seconds after connecting. So, during boot, we start
        // the CP later, to allow a quick NTP update.
        if(!deferConfigPortal) {
            wm.startWebPortal();
        }

        // Allow modem sleep:
        // WIFI_PS_MIN_MODEM is the default, and activated when calling this
        // with "true". When this is enabled, received WiFi data can be
        // delayed for as long as the DTIM period.
        // Since it is the default setting, so no need to call it here.
        //WiFi.setSleep(true);

        // Disable modem sleep, don't want delays accessing the CP or
        // with MQTT.
        WiFi.setSleep(false);

        // Set transmit power to max; we might be connecting as STA after
        // a previous period in AP mode.
        #ifdef SID_DBG
        {
            wifi_power_t power = WiFi.getTxPower();
            Serial.printf("WiFi: Max TX power in STA mode %d\n", power);
        }
        #endif
        WiFi.setTxPower(WIFI_POWER_19_5dBm);

        wifiInAPMode = false;
        wifiIsOff = false;
        wifiOnNow = millis();
        wifiAPIsOff = false;  // Sic! Allows checks like if(wifiAPIsOff || wifiIsOff)

    } else {
        #ifdef SID_DBG
        Serial.println(F("Config portal running in AP-mode"));
        #endif

        {
            #ifdef SID_DBG
            int8_t power;
            esp_wifi_get_max_tx_power(&power);
            Serial.printf("WiFi: Max TX power %d\n", power);
            #endif

            // Try to avoid "burning" the ESP when the WiFi mode
            // is "AP" and the speed/vol knob is fully up by reducing
            // the max. transmit power.
            // The choices are:
            // WIFI_POWER_19_5dBm    = 19.5dBm
            // WIFI_POWER_19dBm      = 19dBm
            // WIFI_POWER_18_5dBm    = 18.5dBm
            // WIFI_POWER_17dBm      = 17dBm
            // WIFI_POWER_15dBm      = 15dBm
            // WIFI_POWER_13dBm      = 13dBm
            // WIFI_POWER_11dBm      = 11dBm
            // WIFI_POWER_8_5dBm     = 8.5dBm
            // WIFI_POWER_7dBm       = 7dBm     <-- proven to avoid the issues
            // WIFI_POWER_5dBm       = 5dBm
            // WIFI_POWER_2dBm       = 2dBm
            // WIFI_POWER_MINUS_1dBm = -1dBm
            WiFi.setTxPower(WIFI_POWER_7dBm);

            #ifdef SID_DBG
            esp_wifi_get_max_tx_power(&power);
            Serial.printf("WiFi: Max TX power set to %d\n", power);
            #endif
        }

        wifiInAPMode = true;
        wifiAPIsOff = false;
        wifiAPModeNow = millis();
        wifiIsOff = false;    // Sic!
    }
}

void wifiOff()
{
    if( (!wifiInAPMode && wifiIsOff) ||
        (wifiInAPMode && wifiAPIsOff) ) {
        return;
    }

    wm.stopWebPortal();
    wm.disconnect();
    WiFi.mode(WIFI_OFF);
}

void wifiOn(unsigned long newDelay, bool alsoInAPMode, bool deferCP)
{
    unsigned long desiredDelay;
    unsigned long Now = millis();

    if(wifiInAPMode && !alsoInAPMode) return;

    if(wifiInAPMode) {
        if(wifiAPOffDelay == 0) return;   // If no delay set, auto-off is disabled
        wifiAPModeNow = Now;              // Otherwise: Restart timer
        if(!wifiAPIsOff) return;
    } else {
        if(origWiFiOffDelay == 0) return; // If no delay set, auto-off is disabled
        desiredDelay = (newDelay > 0) ? newDelay : origWiFiOffDelay;
        if((Now - wifiOnNow >= wifiOffDelay) ||                    // If delay has run out, or
           (wifiOffDelay - (Now - wifiOnNow))  < desiredDelay) {   // new delay exceeds remaining delay:
            wifiOffDelay = desiredDelay;                           // Set new timer delay, and
            wifiOnNow = Now;                                       // restart timer
            #ifdef SID_DBG
            Serial.printf("Restarting WiFi-off timer; delay %d\n", wifiOffDelay);
            #endif
        }
        if(!wifiIsOff) {
            // If WiFi is not off, check if user wanted
            // to start the CP, and do so, if not running
            if(!deferCP) {
                if(!wm.getWebPortalActive()) {
                    wm.startWebPortal();
                }
            }
            return;
        }
    }

    WiFi.mode(WIFI_MODE_STA);

    wifiConnect(deferCP);
}

// Check if WiFi is on; used to determine if a 
// longer interruption due to a re-connect is to
// be expected.
bool wifiIsOn()
{
    if(wifiInAPMode) {
        if(wifiAPOffDelay == 0) return true;
        if(!wifiAPIsOff) return true;
    } else {
        if(origWiFiOffDelay == 0) return true;
        if(!wifiIsOff) return true;
    }
    return false;
}

void wifiStartCP()
{
    if(wifiInAPMode || wifiIsOff)
        return;

    wm.startWebPortal();
}

// This is called when the WiFi config changes, so it has
// nothing to do with our settings here. Despite that,
// we write out our config file so that when the user initially
// configures WiFi, a default settings file exists upon reboot.
// Also, this triggers a reboot, so if the user entered static
// IP data, it becomes active after this reboot.
static void saveConfigCallback()
{
    shouldSaveConfig = 1;
}

// This is the callback from the actual Params page. In this
// case, we really read out the server parms and save them.
static void saveParamsCallback()
{
    shouldSaveConfig = 2;
}

// This is called before a firmware updated is initiated.
// Disable WiFi-off-timers.
static void preUpdateCallback()
{
    wifiAPOffDelay = 0;
    origWiFiOffDelay = 0;

    flushDelayedSave();

    showWaitSequence(true);
}

// Grab static IP parameters from WiFiManager's server.
// Since there is no public method for this, we steal
// the html form parameters in this callback.
static void preSaveConfigCallback()
{
    char ipBuf[20] = "";
    char gwBuf[20] = "";
    char snBuf[20] = "";
    char dnsBuf[20] = "";
    bool invalConf = false;

    #ifdef SID_DBG
    Serial.println("preSaveConfigCallback");
    #endif

    // clear as strncpy might leave us unterminated
    memset(ipBuf, 0, 20);
    memset(gwBuf, 0, 20);
    memset(snBuf, 0, 20);
    memset(dnsBuf, 0, 20);

    if(wm.server->arg(FPSTR(S_ip)) != "") {
        strncpy(ipBuf, wm.server->arg(FPSTR(S_ip)).c_str(), 19);
    } else invalConf |= true;
    if(wm.server->arg(FPSTR(S_gw)) != "") {
        strncpy(gwBuf, wm.server->arg(FPSTR(S_gw)).c_str(), 19);
    } else invalConf |= true;
    if(wm.server->arg(FPSTR(S_sn)) != "") {
        strncpy(snBuf, wm.server->arg(FPSTR(S_sn)).c_str(), 19);
    } else invalConf |= true;
    if(wm.server->arg(FPSTR(S_dns)) != "") {
        strncpy(dnsBuf, wm.server->arg(FPSTR(S_dns)).c_str(), 19);
    } else invalConf |= true;

    #ifdef SID_DBG
    if(strlen(ipBuf) > 0) {
        Serial.printf("IP:%s / SN:%s / GW:%s / DNS:%s\n", ipBuf, snBuf, gwBuf, dnsBuf);
    } else {
        Serial.println("Static IP unset, using DHCP");
    }
    #endif

    if(!invalConf && isIp(ipBuf) && isIp(gwBuf) && isIp(snBuf) && isIp(dnsBuf)) {

        #ifdef SID_DBG
        Serial.println("All IPs valid");
        #endif

        strcpy(ipsettings.ip, ipBuf);
        strcpy(ipsettings.gateway, gwBuf);
        strcpy(ipsettings.netmask, snBuf);
        strcpy(ipsettings.dns, dnsBuf);

        shouldSaveIPConfig = true;

    } else {

        #ifdef SID_DBG
        if(strlen(ipBuf) > 0) {
            Serial.println("Invalid IP");
        }
        #endif

        shouldDeleteIPConfig = true;

    }
}

static void setupStaticIP()
{
    IPAddress ip;
    IPAddress gw;
    IPAddress sn;
    IPAddress dns;

    if(strlen(ipsettings.ip) > 0 &&
        isIp(ipsettings.ip) &&
        isIp(ipsettings.gateway) &&
        isIp(ipsettings.netmask) &&
        isIp(ipsettings.dns)) {

        ip = stringToIp(ipsettings.ip);
        gw = stringToIp(ipsettings.gateway);
        sn = stringToIp(ipsettings.netmask);
        dns = stringToIp(ipsettings.dns);

        wm.setSTAStaticIPConfig(ip, gw, sn, dns);
    }
}

void updateConfigPortalValues()
{
    const char custHTMLSel[] = " selected";

    // Make sure the settings form has the correct values

    custom_ssDelay.setValue(settings.ssTimer, 3);

    custom_hostName.setValue(settings.hostName, 31);
    custom_sysID.setValue(settings.systemID, 7);
    custom_appw.setValue(settings.appw, 8);
    custom_wifiConTimeout.setValue(settings.wifiConTimeout, 2);
    custom_wifiConRetries.setValue(settings.wifiConRetries, 2);

    custom_tcdIP.setValue(settings.tcdIP, 63);

    #ifdef SID_HAVEMQTT
    custom_mqttServer.setValue(settings.mqttServer, 79);
    custom_mqttUser.setValue(settings.mqttUser, 63);
    #endif

    #ifdef TC_NOCHECKBOXES  // Standard text boxes: -------

    custom_bootSA.setValue(settings.bootSA, 1);

    custom_disDIR.setValue(settings.disDIR, 1);

    custom_TCDpresent.setValue(settings.TCDpresent, 1);
    custom_noETTOL.setValue(settings.noETTOLead, 1);

    custom_uGPS.setValue(settings.useGPSS, 1);
    custom_uNM.setValue(settings.useNM, 1);
    custom_uFPO.setValue(settings.useFPO, 1);
    custom_bttfnTT.setValue(settings.bttfnTT, 1);
    custom_ssClock.setValue(settings.ssClock, 1);
    custom_ssClockO.setValue(settings.ssClockOffNM, 1);

    custom_sTTANI.setValue(settings.skipTTAnim, 1);
    custom_SApeaks.setValue(settings.SApeaks, 1);

    #ifdef SID_HAVEMQTT
    custom_useMQTT.setValue(settings.useMQTT, 1);
    #endif
    
    custom_CfgOnSD.setValue(settings.CfgOnSD, 1);
    //custom_sdFrq.setValue(settings.sdFreq, 1);
    
    #else   // For checkbox hack --------------------------

    setCBVal(&custom_bootSA, settings.bootSA);

    setCBVal(&custom_disDIR, settings.disDIR);

    setCBVal(&custom_TCDpresent, settings.TCDpresent);
    setCBVal(&custom_noETTOL, settings.noETTOLead);

    setCBVal(&custom_uGPS, settings.useGPSS);
    setCBVal(&custom_uNM, settings.useNM);
    setCBVal(&custom_uFPO, settings.useFPO);
    setCBVal(&custom_bttfnTT, settings.bttfnTT);
    setCBVal(&custom_ssClock, settings.ssClock);
    setCBVal(&custom_ssClockO, settings.ssClockOffNM);

    setCBVal(&custom_sTTANI, settings.skipTTAnim);
    setCBVal(&custom_SApeaks, settings.SApeaks);

    #ifdef SID_HAVEMQTT
    setCBVal(&custom_useMQTT, settings.useMQTT);
    #endif
    
    setCBVal(&custom_CfgOnSD, settings.CfgOnSD);
    //setCBVal(&custom_sdFrq, settings.sdFreq);

    #endif // ---------------------------------------------

    updateConfigPortalStrictValue();
}

void updateConfigPortalStrictValue()
{
    strcpy(settings.strictMode, strictMode ? "1" : "0");
    #ifdef PG_NOCHECKBOXES  // Standard text boxes: -------
    custom_sStrict.setValue(settings.strictMode, 1);
    #else   // For checkbox hack --------------------------
    setCBVal(&custom_sStrict, settings.strictMode);
    #endif // ---------------------------------------------
}

bool wifi_getIP(uint8_t& a, uint8_t& b, uint8_t& c, uint8_t& d)
{
    IPAddress myip;

    switch(WiFi.getMode()) {
      case WIFI_MODE_STA:
          myip = WiFi.localIP();
          break;
      case WIFI_MODE_AP:
      case WIFI_MODE_APSTA:
          myip = WiFi.softAPIP();
          break;
      default:
          a = b = c = d = 0;
          return true;
    }

    a = myip[0];
    b = myip[1];
    c = myip[2];
    d = myip[3];

    return true;
}

// Check if String is a valid IP address
bool isIp(char *str)
{
    int segs = 0;
    int digcnt = 0;
    int num = 0;

    while(*str != '\0') {

        if(*str == '.') {

            if(!digcnt || (++segs == 4))
                return false;

            num = digcnt = 0;
            str++;
            continue;

        } else if((*str < '0') || (*str > '9')) {

            return false;

        }

        if((num = (num * 10) + (*str - '0')) > 255)
            return false;

        digcnt++;
        str++;
    }

    if(segs == 3) 
        return true;

    return false;
}

// IPAddress to string
static void ipToString(char *str, IPAddress ip)
{
    sprintf(str, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

// String to IPAddress
static IPAddress stringToIp(char *str)
{
    int ip1, ip2, ip3, ip4;

    sscanf(str, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4);

    return IPAddress(ip1, ip2, ip3, ip4);
}

/*
 * Read parameter from server, for customhmtl input
 */
static void getParam(String name, char *destBuf, size_t length)
{
    memset(destBuf, 0, length+1);
    if(wm.server->hasArg(name)) {
        strncpy(destBuf, wm.server->arg(name).c_str(), length);
    }
}

static bool myisspace(char mychar)
{
    return (mychar == ' ' || mychar == '\n' || mychar == '\t' || mychar == '\v' || mychar == '\f' || mychar == '\r');
}

static bool myisgoodchar(char mychar)
{
    return ((mychar >= '0' && mychar <= '9') || (mychar >= 'a' && mychar <= 'z') || (mychar >= 'A' && mychar <= 'Z') || mychar == '-');
}

static char* strcpytrim(char* destination, const char* source, bool doFilter)
{
    char *ret = destination;
    
    while(*source) {
        if(!myisspace(*source) && (!doFilter || myisgoodchar(*source))) *destination++ = *source;
        source++;
    }
    
    *destination = 0;
    
    return ret;
}

static void mystrcpy(char *sv, WiFiManagerParameter *el)
{
    strcpy(sv, el->getValue());
}

#ifndef TC_NOCHECKBOXES
static void strcpyCB(char *sv, WiFiManagerParameter *el)
{
    strcpy(sv, (atoi(el->getValue()) > 0) ? "1" : "0");
}

static void setCBVal(WiFiManagerParameter *el, char *sv)
{
    const char makeCheck[] = "1' checked a='";
    
    el->setValue((atoi(sv) > 0) ? makeCheck : "1", 14);
}
#endif

#ifdef SID_HAVEMQTT
static void strcpyutf8(char *dst, const char *src, unsigned int len)
{
    strncpy(dst, src, len - 1);
    dst[len - 1] = 0;
}

static void mqttLooper()
{
}

static void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    int i = 0, j, ml = (length <= 255) ? length : 255;
    char tempBuf[256];
    static const char *cmdList[] = {
      "TIMETRAVEL",       // 0
      "IDLE_0",           // 1
      "IDLE_1",           // 2
      "IDLE_2",           // 3
      "IDLE_3",           // 4
      "IDLE_4",           // 5
      "IDLE_5",           // 6
      "IDLE",             // 7
      "SA",               // 8
      NULL
    };
    static const char *cmdList2[] = {
      "PREPARE",          // 0
      "TIMETRAVEL",       // 1
      "REENTRY",          // 2
      "ABORT_TT",         // 3
      "ALARM",            // 4
      "WAKEUP",           // 5
      NULL
    };

    if(!length) return;

    memcpy(tempBuf, (const char *)payload, ml);
    tempBuf[ml] = 0;
    for(j = 0; j < ml; j++) {
        if(tempBuf[j] >= 'a' && tempBuf[j] <= 'z') tempBuf[j] &= ~0x20;
    }

    if(!strcmp(topic, "bttf/tcd/pub")) {

        // Commands from TCD or other props

        while(cmdList2[i]) {
            j = strlen(cmdList2[i]);
            if((length >= j) && !strncmp((const char *)tempBuf, cmdList2[i], j)) {
                break;
            }
            i++;          
        }

        if(!cmdList2[i]) return;

        switch(i) {
        case 0:
            // Prepare for TT. Comes at some undefined point,
            // an undefined time before the actual tt, and may
            // now come at all.
            // We disable our Screen Saver.
            // We don't ignore this if TCD is connected by wire,
            // because this signal does not come via wire.
            if(!TTrunning && !IRLearning) {
                prepareTT();
            }
            break;
        case 1:
            // Trigger Time Travel (if not running already)
            // Ignore command if TCD is connected by wire
            if(!TCDconnected && !TTrunning && !IRLearning) {
                networkTimeTravel = true;
                networkTCDTT = true;
                networkReentry = false;
                networkAbort = false;
                networkLead = ETTO_LEAD;
            }
            break;
        case 2:   // Re-entry
            // Start re-entry (if TT currently running)
            // Ignore command if TCD is connected by wire
            if(!TCDconnected && TTrunning && networkTCDTT) {
                networkReentry = true;
            }
            break;
        case 3:   // Abort TT (TCD fake-powered down during TT)
            // Ignore command if TCD is connected by wire
            // (mainly because this is no network-triggered TT)
            if(!TCDconnected && TTrunning && networkTCDTT) {
                networkAbort = true;
            }
            break;
        case 4:
            networkAlarm = true;
            // Eval this at our convenience
            break;
        case 5:
            if(!TTrunning && !IRLearning) {
                wakeup();
            }
            break;
        }
       
    } else if(!strcmp(topic, "bttf/sid/cmd")) {

        // User commands

        // Not taking commands under these circumstances:
        if(TTrunning || IRLearning || !FPBUnitIsOn)
            return;

        while(cmdList[i]) {
            j = strlen(cmdList[i]);
            if((length >= j) && !strncmp((const char *)tempBuf, cmdList[i], j)) {
                break;
            }
            i++;          
        }

        if(!cmdList[i]) return;

        switch(i) {
        case 0:
            // trigger Time Travel; treated like button, not
            // like TT from TCD.
            networkTimeTravel = true;
            networkTCDTT = false;
            break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            setIdleMode(i - 1);
            break;
        case 7:
            switch_to_idle();
            break;
        case 8:
            switch_to_sa();
            break;
        }
            
    } 
}

#ifdef SID_DBG
#define MQTT_FAILCOUNT 6
#else
#define MQTT_FAILCOUNT 120
#endif

static void mqttPing()
{
    switch(mqttClient.pstate()) {
    case PING_IDLE:
        if(WiFi.status() == WL_CONNECTED) {
            if(!mqttPingNow || (millis() - mqttPingNow > mqttPingInt)) {
                mqttPingNow = millis();
                if(!mqttClient.sendPing()) {
                    // Mostly fails for internal reasons;
                    // skip ping test in that case
                    mqttDoPing = false;
                    mqttPingDone = true;  // allow mqtt-connect attempt
                }
            }
        }
        break;
    case PING_PINGING:
        if(mqttClient.pollPing()) {
            mqttPingDone = true;          // allow mqtt-connect attempt
            mqttPingNow = 0;
            mqttPingsExpired = 0;
            mqttPingInt = MQTT_SHORT_INT; // Overwritten on fail in reconnect
            // Delay re-connection for 5 seconds after first ping echo
            mqttReconnectNow = millis() - (mqttReconnectInt - 5000);
        } else if(millis() - mqttPingNow > 5000) {
            mqttClient.cancelPing();
            mqttPingNow = millis();
            mqttPingsExpired++;
            mqttPingInt = MQTT_SHORT_INT * (1 << (mqttPingsExpired / MQTT_FAILCOUNT));
            mqttReconnFails = 0;
        }
        break;
    } 
}

static bool mqttReconnect(bool force)
{
    bool success = false;

    if(useMQTT && (WiFi.status() == WL_CONNECTED)) {

        if(!mqttClient.connected()) {
    
            if(force || !mqttReconnectNow || (millis() - mqttReconnectNow > mqttReconnectInt)) {

                #ifdef SID_DBG
                Serial.println("MQTT: Attempting to (re)connect");
                #endif
    
                if(strlen(mqttUser)) {
                    success = mqttClient.connect(settings.hostName, mqttUser, strlen(mqttPass) ? mqttPass : NULL);
                } else {
                    success = mqttClient.connect(settings.hostName);
                }
    
                mqttReconnectNow = millis();
                
                if(!success) {
                    mqttRestartPing = true;  // Force PING check before reconnection attempt
                    mqttReconnFails++;
                    if(mqttDoPing) {
                        mqttPingInt = MQTT_SHORT_INT * (1 << (mqttReconnFails / MQTT_FAILCOUNT));
                    } else {
                        mqttReconnectInt = MQTT_SHORT_INT * (1 << (mqttReconnFails / MQTT_FAILCOUNT));
                    }
                    #ifdef SID_DBG
                    Serial.printf("MQTT: Failed to reconnect (%d)\n", mqttReconnFails);
                    #endif
                } else {
                    mqttReconnFails = 0;
                    mqttReconnectInt = MQTT_SHORT_INT;
                    #ifdef SID_DBG
                    Serial.println("MQTT: Connected to broker, waiting for CONNACK");
                    #endif
                }
    
                return success;
            } 
        }
    }
      
    return true;
}

static void mqttSubscribe()
{
    // Meant only to be called when connected!
    if(!mqttSubAttempted) {
        if(!mqttClient.subscribe("bttf/sid/cmd", "bttf/tcd/pub")) {
            #ifdef SID_DBG
            Serial.println("MQTT: Failed to subscribe to command topics");
            #endif
        }
        mqttSubAttempted = true;
    }
}

bool mqttState()
{
    return (useMQTT && mqttClient.connected());
}

void mqttPublish(const char *topic, const char *pl, unsigned int len)
{
    if(useMQTT) {
        mqttClient.publish(topic, (uint8_t *)pl, len, false);
    }
}           

#endif
