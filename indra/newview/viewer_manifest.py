#!/usr/bin/env python
"""\
@file viewer_manifest.py
@author Ryan Williams
@brief Description of all installer viewer files, and methods for packaging
       them into installers for all supported platforms.

$LicenseInfo:firstyear=2006&license=viewerlgpl$
Second Life Viewer Source Code
Copyright (C) 2006-2014, Linden Research, Inc.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
version 2.1 of the License only.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
$/LicenseInfo$
"""
import errno
import json
import os
import os.path
import plistlib
import random
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import time
import zipfile

#<FS:AO>
import shlex
import zipfile
#</FS:AO>

from fs_viewer_manifest import FSViewerManifest #<FS:ND/> Manifest extensions for Firestorm

viewer_dir = os.path.dirname(__file__)
# Add indra/lib/python to our path so we don't have to muck with PYTHONPATH.
# Put it FIRST because some of our build hosts have an ancient install of
# indra.util.llmanifest under their system Python!
sys.path.insert(0, os.path.join(viewer_dir, os.pardir, "lib", "python"))
from indra.util.llmanifest import LLManifest, main, path_ancestors, CHANNEL_VENDOR_BASE, RELEASE_CHANNEL, ManifestError
from llbase import llsd

class ViewerManifest(LLManifest,FSViewerManifest):
    def is_packaging_viewer(self):
        # Some commands, files will only be included
        # if we are packaging the viewer on windows.
        # This manifest is also used to copy
        # files during the build (see copy_w_viewer_manifest
        # and copy_l_viewer_manifest targets)
        return 'package' in self.args['actions']
    
    def construct(self):
        super(ViewerManifest, self).construct()
        self.path(src="../../scripts/messages/message_template.msg", dst="app_settings/message_template.msg")
        self.path(src="../../etc/message.xml", dst="app_settings/message.xml")
        
        # <FS:LO> Copy dictionaries to a place where the viewer can find them if ran from visual studio
        pkgdir = os.path.join(self.args['build'], os.pardir, 'packages')
        with self.prefix(src=pkgdir, dst="app_settings"):
            self.path("dictionaries")
        # </FS:LO>

        if self.is_packaging_viewer():
            with self.prefix(src_dst="app_settings"):
                self.exclude("logcontrol.xml")
                self.exclude("logcontrol-dev.xml")
                self.path("*.ini")
                self.path("*.xml")
                self.path("*.db2")

                # include the entire shaders directory recursively
                self.path("shaders")
                # include the extracted list of contributors
                contributions_path = "../../doc/contributions.txt"
                contributor_names = self.extract_names(contributions_path)
                self.put_in_file(contributor_names, "contributors.txt", src=contributions_path)

                # ... and the entire windlight directory
                self.path("windlight")

                # ... and the entire image filters directory
                self.path("filters")
            
                # ... and the included spell checking dictionaries
                # <FS:LO> Copy dictionaries to a place where the viewer can find them if ran from visual studio
                # ... and the included spell checking dictionaries
#                pkgdir = os.path.join(self.args['build'], os.pardir, 'packages')
#                with self.prefix(src=pkgdir):
#                    self.path("dictionaries")
                # </FS:LO>

                # include the entire beams directory
                self.path("beams")
                self.path("beamsColors")

                # include the extracted packages information (see BuildPackagesInfo.cmake)
                self.path(src=os.path.join(self.args['build'],"packages-info.txt"), dst="packages-info.txt")
                # CHOP-955: If we have "sourceid" or "viewer_channel" in the
                # build process environment, generate it into
                # settings_install.xml.
                settings_template = dict(
                    sourceid=dict(Comment='Identify referring agency to Linden web servers',
                                  Persist=1,
                                  Type='String',
                                  Value=''),
                    CmdLineGridChoice=dict(Comment='Default grid',
                                  Persist=0,
                                  Type='String',
                                  Value=''),
                    CmdLineChannel=dict(Comment='Command line specified channel name',
                                        Persist=0,
                                        Type='String',
                                        Value=''))
                settings_install = {}
                sourceid = self.args.get('sourceid')
                if sourceid:
                    settings_install['sourceid'] = settings_template['sourceid'].copy()
                    settings_install['sourceid']['Value'] = sourceid
                    print "Set sourceid in settings_install.xml to '%s'" % sourceid

                if self.args.get('channel_suffix'):
                    settings_install['CmdLineChannel'] = settings_template['CmdLineChannel'].copy()
                    settings_install['CmdLineChannel']['Value'] = self.channel_with_pkg_suffix()
                    print "Set CmdLineChannel in settings_install.xml to '%s'" % self.channel_with_pkg_suffix()

                if self.args.get('grid'):
                    settings_install['CmdLineGridChoice'] = settings_template['CmdLineGridChoice'].copy()
                    settings_install['CmdLineGridChoice']['Value'] = self.grid()
                    print "Set CmdLineGridChoice in settings_install.xml to '%s'" % self.grid()

                # put_in_file(src=) need not be an actual pathname; it
                # only needs to be non-empty
                self.put_in_file(llsd.format_pretty_xml(settings_install),
                                 "settings_install.xml",
                                 src="environment")


            with self.prefix(src_dst="character"):
                self.path("*.llm")
                self.path("*.xml")
                self.path("*.tga")

            # Include our fonts
            with self.prefix(src_dst="fonts"):
                self.path("*.ttf")
                self.path("*.txt")
                self.path("*.xml")
                
            # <FS:AO> Include firestorm resources
            with self.prefix(src_dst="fs_resources"):
                self.path("*.txt")
                self.path("*.lsl")
                self.path("*.lsltxt")

            # skins
            with self.prefix(src_dst="skins"):
                    self.path("skins.xml")
                    # include the entire textures directory recursively
                    with self.prefix(src_dst="*/textures"):
                            self.path("*/*.tga")
                            self.path("*/*.j2c")
                            self.path("*/*.jpg")
                            self.path("*/*.png")
                            self.path("*.tga")
                            self.path("*.j2c")
                            self.path("*.jpg")
                            self.path("*.png")
                            self.path("textures.xml")
                    self.path("*/xui/*/*.xml")
                    self.path("*/xui/*/widgets/*.xml")
                    self.path("*/themes/*/colors.xml")
                    with self.prefix(src="*/themes/*/textures"):
                        self.path("*/*.tga")
                        self.path("*/*.j2c")
                        self.path("*/*.jpg")
                        self.path("*/*.png")
                        self.path("*.tga")
                        self.path("*.j2c")
                        self.path("*.jpg")
                        self.path("*.png")
                    self.path("*/*.xml")

                    # Local HTML files (e.g. loading screen)
                    # The claim is that we never use local html files any
                    # longer. But rather than commenting out this block, let's
                    # rename every html subdirectory as html.old. That way, if
                    # we're wrong, a user actually does have the relevant
                    # files; s/he just needs to rename every html.old
                    # directory back to html to recover them.
                    with self.prefix(src="*/html", dst="*/html.old"):
                            self.path("*.png")
                            self.path("*/*/*.html")
                            self.path("*/*/*.gif")


            # local_assets dir (for pre-cached textures)
            with self.prefix(src_dst="local_assets"):
                self.path("*.j2c")
                self.path("*.tga")

            # File in the newview/ directory
            self.path("gpu_table.txt")

            #build_data.json.  Standard with exception handling is fine.  If we can't open a new file for writing, we have worse problems
            #platform is computed above with other arg parsing
            build_data_dict = {"Type":"viewer","Version":'.'.join(self.args['version']),
                            "Channel Base": CHANNEL_VENDOR_BASE,
                            "Channel":self.channel_with_pkg_suffix(),
                            "Platform":self.build_data_json_platform,
                            "Address Size":self.address_size,
                            "Update Service":"https://update.secondlife.com/update",
                            }
            # Only store this if it's both present and non-empty
            bugsplat_db = self.args.get('bugsplat')
            if bugsplat_db:
                build_data_dict["BugSplat DB"] = bugsplat_db
            build_data_dict = self.finish_build_data_dict(build_data_dict)
            with open(os.path.join(os.pardir,'build_data.json'), 'w') as build_data_handle:
                json.dump(build_data_dict,build_data_handle)

            #we likely no longer need the test, since we will throw an exception above, but belt and suspenders and we get the
            #return code for free.
            if not self.path2basename(os.pardir, "build_data.json"):
                print "No build_data.json file"

    def finish_build_data_dict(self, build_data_dict):
        return build_data_dict

    def grid(self):
        return self.args['grid']

    def channel(self):
        return self.args['channel']

    def channel_with_pkg_suffix(self):
        fullchannel=self.channel()
        channel_suffix = self.args.get('channel_suffix')
        if channel_suffix:
            fullchannel+=' '+channel_suffix
        return fullchannel

    def channel_variant(self):
        global CHANNEL_VENDOR_BASE
        return self.channel().replace(CHANNEL_VENDOR_BASE, "").strip()

    def channel_type(self): # returns 'release', 'beta', 'project', or 'test'
        channel_qualifier=self.channel_variant().lower()
        if channel_qualifier.startswith('release'):
            channel_type='release'
        elif channel_qualifier.startswith('beta'):
            channel_type='beta'
        #<FS:TS> Use our more-or-less-standard channel types instead of LL's
        #elif channel_qualifier.startswith('project'):
        #    channel_type='project'
        #else:
        #    channel_type='test'
        elif channel_qualifier.startswith('nightly'):
            channel_type='nightly'
        else:
            channel_type='private'
        return channel_type

    def channel_variant_app_suffix(self):
        # get any part of the channel name after the CHANNEL_VENDOR_BASE
        suffix=self.channel_variant()
        # by ancient convention, we don't use Release in the app name
        if self.channel_type() == 'release':
            suffix=suffix.replace('Release', '').strip()
        # for the base release viewer, suffix will now be null - for any other, append what remains
        if suffix:
            #suffix = "_".join([''] + suffix.split())
            suffix = "_".join(suffix.split()) # <FS> Don't prepend underscore before suffix
        # the additional_packages mechanism adds more to the installer name (but not to the app name itself)
        # ''.split() produces empty list, so suffix only changes if
        # channel_suffix is non-empty
        suffix = "_".join([suffix] + self.args.get('channel_suffix', '').split())
        return suffix

    def installer_base_name(self):
        global CHANNEL_VENDOR_BASE
        # a standard map of strings for replacing in the templates
        #<FS:TS> tag "OS" after CHANNEL_VENDOR_BASE and before any suffix
        channel_base = "Phoenix-" + CHANNEL_VENDOR_BASE
        if self.fs_is_opensim():
            channel_base = channel_base + "OS"
        #</FS:TS>
        substitution_strings = {
            'channel_vendor_base' : '_'.join(channel_base.split()),
            'channel_variant_underscores':self.channel_variant_app_suffix(),
            'version_underscores' : '_'.join(self.args['version']),
            'arch':self.args['arch']
            }
        return "%(channel_vendor_base)s%(channel_variant_underscores)s_%(version_underscores)s_%(arch)s" % substitution_strings

    def app_name(self):
        global CHANNEL_VENDOR_BASE
        channel_type=self.channel_type()
        if channel_type == 'release':
            app_suffix='Viewer'
        else:
            app_suffix=self.channel_variant()

        #<FS:ND> tag "OS" after CHANNEL_VENDOR_BASE and before any suffix
        if self.fs_is_opensim():
            app_suffix = "OS" + app_suffix
        #</FS:ND>

        #<FS:ND> Don't separate name by whitespace. This break a lot of things in the old FS installer logic.
        #return CHANNEL_VENDOR_BASE + ' ' + app_suffix
        return CHANNEL_VENDOR_BASE + app_suffix
        #</FS:ND>

    def app_name_oneword(self):
        return ''.join(self.app_name().split())
    
    def icon_path(self):
        # <FS:ND> Add -os for oss builds
        if self.fs_is_opensim():
            return "icons/" + self.channel_type() + "-os"
        # </FS:ND>
        return "icons/" + self.channel_type()

    def extract_names(self,src):
        try:
            contrib_file = open(src,'r')
        except IOError:
            print "Failed to open '%s'" % src
            raise
        lines = contrib_file.readlines()
        contrib_file.close()

        # All lines up to and including the first blank line are the file header; skip them
        lines.reverse() # so that pop will pull from first to last line
        while not re.match("\s*$", lines.pop()) :
            pass # do nothing

        # A line that starts with a non-whitespace character is a name; all others describe contributions, so collect the names
        names = []
        for line in lines :
            if re.match("\S", line) :
                names.append(line.rstrip())
        # It's not fair to always put the same people at the head of the list
        random.shuffle(names)
        return ', '.join(names)

    def relsymlinkf(self, src, dst=None, catch=True):
        """
        relsymlinkf() is just like symlinkf(), but instead of requiring the
        caller to pass 'src' as a relative pathname, this method expects 'src'
        to be absolute, and creates a symlink whose target is the relative
        path from 'src' to dirname(dst).
        """
        dstdir, dst = self._symlinkf_prep_dst(src, dst)

        # Determine the relative path starting from the directory containing
        # dst to the intended src.
        src = self.relpath(src, dstdir)

        self._symlinkf(src, dst, catch)
        return dst

    def symlinkf(self, src, dst=None, catch=True):
        """
        Like ln -sf, but uses os.symlink() instead of running ln. This creates
        a symlink at 'dst' that points to 'src' -- see:
        https://docs.python.org/2/library/os.html#os.symlink

        If you omit 'dst', this creates a symlink with basename(src) at
        get_dst_prefix() -- in other words: put a symlink to this pathname
        here at the current dst prefix.

        'src' must specifically be a *relative* symlink. It makes no sense to
        create an absolute symlink pointing to some path on the build machine!

        Also:
        - We prepend 'dst' with the current get_dst_prefix(), so it has similar
          meaning to associated self.path() calls.
        - We ensure that the containing directory os.path.dirname(dst) exists
          before attempting the symlink.

        If you pass catch=False, exceptions will be propagated instead of
        caught.
        """
        dstdir, dst = self._symlinkf_prep_dst(src, dst)
        self._symlinkf(src, dst, catch)
        return dst

    def _symlinkf_prep_dst(self, src, dst):
        # helper for relsymlinkf() and symlinkf()
        if dst is None:
            dst = os.path.basename(src)
        dst = os.path.join(self.get_dst_prefix(), dst)
        # Seems silly to prepend get_dst_prefix() to dst only to call
        # os.path.dirname() on it again, but this works even when the passed
        # 'dst' is itself a pathname.
        dstdir = os.path.dirname(dst)
        self.cmakedirs(dstdir)
        return (dstdir, dst)

    def _symlinkf(self, src, dst, catch):
        # helper for relsymlinkf() and symlinkf()
        # the passed src must be relative
        if os.path.isabs(src):
            raise ManifestError("Do not symlinkf(absolute %r, asis=True)" % src)

        # The outer catch is the one that reports failure even after attempted
        # recovery.
        try:
            # At the inner layer, recovery may be possible.
            try:
                os.symlink(src, dst)
            except OSError as err:
                if err.errno != errno.EEXIST:
                    raise
                # We could just blithely attempt to remove and recreate the target
                # file, but that strategy doesn't work so well if we don't have
                # permissions to remove it. Check to see if it's already the
                # symlink we want, which is the usual reason for EEXIST.
                elif os.path.islink(dst):
                    if os.readlink(dst) == src:
                        # the requested link already exists
                        pass
                    else:
                        # dst is the wrong symlink; attempt to remove and recreate it
                        os.remove(dst)
                        os.symlink(src, dst)
                elif os.path.isdir(dst):
                    print "Requested symlink (%s) exists but is a directory; replacing" % dst
                    shutil.rmtree(dst)
                    os.symlink(src, dst)
                elif os.path.exists(dst):
                    print "Requested symlink (%s) exists but is a file; replacing" % dst
                    os.remove(dst)
                    os.symlink(src, dst)
                else:
                    # out of ideas
                    raise
        except Exception as err:
            # report
            print "Can't symlink %r -> %r: %s: %s" % \
                  (dst, src, err.__class__.__name__, err)
            # if caller asked us not to catch, re-raise this exception
            if not catch:
                raise

    def relpath(self, path, base=None, symlink=False):
        """
        Return the relative path from 'base' to the passed 'path'. If base is
        omitted, self.get_dst_prefix() is assumed. In other words: make a
        same-name symlink to this path right here in the current dest prefix.

        Normally we resolve symlinks. To retain symlinks, pass symlink=True.
        """
        if base is None:
            base = self.get_dst_prefix()

        # Since we use os.path.relpath() for this, which is purely textual, we
        # must ensure that both pathnames are absolute.
        if symlink:
            # symlink=True means: we know path is (or indirects through) a
            # symlink, don't resolve, we want to use the symlink.
            abspath = os.path.abspath
        else:
            # symlink=False means to resolve any symlinks we may find
            abspath = os.path.realpath

        return os.path.relpath(abspath(path), abspath(base))


class WindowsManifest(ViewerManifest):
    # We want the platform, per se, for every Windows build to be 'win'. The
    # VMP will concatenate that with the address_size.
    build_data_json_platform = 'win'

    def final_exe(self):
        return self.app_name_oneword()+".exe"

    def finish_build_data_dict(self, build_data_dict):
        #MAINT-7294: Windows exe names depend on channel name, so write that in also
        build_data_dict.update({'Executable':self.final_exe()})
        return build_data_dict

    def test_msvcrt_and_copy_action(self, src, dst):
        # This is used to test a dll manifest.
        # It is used as a temporary override during the construct method
        from test_win32_manifest import test_assembly_binding
        # TODO: This is redundant with LLManifest.copy_action(). Why aren't we
        # calling copy_action() in conjunction with test_assembly_binding()?
        if src and (os.path.exists(src) or os.path.islink(src)):
            # ensure that destination path exists
            self.cmakedirs(os.path.dirname(dst))
            self.created_paths.append(dst)
            if not os.path.isdir(src):
                if(self.args['configuration'].lower() == 'debug'):
                    test_assembly_binding(src, "Microsoft.VC80.DebugCRT", "8.0.50727.4053")
                else:
                    test_assembly_binding(src, "Microsoft.VC80.CRT", "8.0.50727.4053")
                self.ccopy(src,dst)
            else:
                raise Exception("Directories are not supported by test_CRT_and_copy_action()")
        else:
            print "Doesn't exist:", src

    def test_for_no_msvcrt_manifest_and_copy_action(self, src, dst):
        # This is used to test that no manifest for the msvcrt exists.
        # It is used as a temporary override during the construct method
        from test_win32_manifest import test_assembly_binding
        from test_win32_manifest import NoManifestException, NoMatchingAssemblyException
        # TODO: This is redundant with LLManifest.copy_action(). Why aren't we
        # calling copy_action() in conjunction with test_assembly_binding()?
        if src and (os.path.exists(src) or os.path.islink(src)):
            # ensure that destination path exists
            self.cmakedirs(os.path.dirname(dst))
            self.created_paths.append(dst)
            if not os.path.isdir(src):
                try:
                    if(self.args['configuration'].lower() == 'debug'):
                        test_assembly_binding(src, "Microsoft.VC80.DebugCRT", "")
                    else:
                        test_assembly_binding(src, "Microsoft.VC80.CRT", "")
                    raise Exception("Unknown condition")
                except NoManifestException as err:
                    pass
                except NoMatchingAssemblyException as err:
                    pass
                    
                self.ccopy(src,dst)
            else:
                raise Exception("Directories are not supported by test_CRT_and_copy_action()")
        else:
            print "Doesn't exist:", src
        
    def construct(self):
        super(WindowsManifest, self).construct()

        pkgdir = os.path.join(self.args['build'], os.pardir, 'packages')
        relpkgdir = os.path.join(pkgdir, "lib", "release")
        debpkgdir = os.path.join(pkgdir, "lib", "debug")

        if self.is_packaging_viewer():
            # Find secondlife-bin.exe in the 'configuration' dir, then rename it to the result of final_exe.
            self.path(src='%s/firestorm-bin.exe' % self.args['configuration'], dst=self.final_exe())

            # <FS:Ansariel> Remove VMP
            #with self.prefix(src=os.path.join(pkgdir, "VMP")):
                # include the compiled launcher scripts so that it gets included in the file_list
            #    self.path('SL_Launcher.exe')
                #IUM is not normally executed directly, just imported.  No exe needed.
            #    self.path("InstallerUserMessage.py")

            #with self.prefix(dst="vmp_icons"):
            #    with self.prefix(src=self.icon_path()):
            #        self.path("secondlife.ico")
                #VMP  Tkinter icons
            #    with self.prefix(src="vmp_icons"):
            #        self.path("*.png")
            #        self.path("*.gif")

            #before, we only needed llbase at build time.  With VMP, we need it at run time.
            #with self.prefix(src=os.path.join(pkgdir, "lib", "python", "llbase"), dst="llbase"):
            #    self.path("*.py")
            #    self.path("_cllsd.so")
            # </FS:Ansariel> Remove VMP

        # Plugin host application
        self.path2basename(os.path.join(os.pardir,
                                        'llplugin', 'slplugin', self.args['configuration']),
                           "slplugin.exe")
        
        # Get shared libs from the shared libs staging directory
        with self.prefix(src=os.path.join(self.args['build'], os.pardir,
                                          'sharedlibs', self.args['configuration'])):

            # Get llcommon and deps. If missing assume static linkage and continue.
            try:
                self.path('llcommon.dll')
                self.path('libapr-1.dll')
                self.path('libaprutil-1.dll')
                self.path('libapriconv-1.dll')
                
            except RuntimeError as err:
                print err.message
                print "Skipping llcommon.dll (assuming llcommon was linked statically)"

            # Mesh 3rd party libs needed for auto LOD and collada reading
            try:
                self.path("glod.dll")
            except RuntimeError as err:
                print err.message
                print "Skipping GLOD library (assumming linked statically)"

            # Get fmodstudio dll, continue if missing
            try:
                if self.args['configuration'].lower() == 'debug':
                    if(self.address_size == 64):
                        self.path("fmodL64.dll")
                    else:
                        self.path("fmodL.dll")
                else:
                    if(self.address_size == 64):
                        self.path("fmod64.dll")
                    else:
                        self.path("fmod.dll")
            except:
                print "Skipping fmodstudio audio library (assuming other audio engine)"

            # Get fmodex dll, continue if missing
            try:
                if(self.address_size == 64):
                    self.path("fmodex64.dll")
                else:
                    self.path("fmodex.dll")
            except:
                print "Skipping fmodex audio library (assuming other audio engine)"

            # For textures
            self.path("openjpeg.dll")

            # These need to be installed as a SxS assembly, currently a 'private' assembly.
            # See http://msdn.microsoft.com/en-us/library/ms235291(VS.80).aspx
            if self.args['configuration'].lower() == 'debug':
                self.path("msvcr120d.dll")
                self.path("msvcp120d.dll")
            else:
                self.path("msvcr120.dll")
                self.path("msvcp120.dll")

            # Vivox runtimes
            self.path("SLVoice.exe")
            if (self.address_size == 64):
                self.path("vivoxsdk_x64.dll")
                self.path("ortp_x64.dll")
            else:
                self.path("vivoxsdk.dll")
                self.path("ortp.dll")
            self.path("libsndfile-1.dll")
            self.path("vivoxoal.dll")
            
            # Security
            self.path("ssleay32.dll")
            self.path("libeay32.dll")

            # HTTP/2
            self.path("nghttp2.dll")

            # Hunspell
            self.path("libhunspell.dll")

            # BugSplat
            if self.args.get('bugsplat'):
                if(self.address_size == 64):
                    self.path("BsSndRpt64.exe")
                    self.path("BugSplat64.dll")
                    self.path("BugSplatRc64.dll")
                else:
                    self.path("BsSndRpt.exe")
                    self.path("BugSplat.dll")
                    self.path("BugSplatRc.dll")

            # Growl
            self.path("growl.dll")
            self.path("growl++.dll")

            # <FS:ND> Copy symbols for breakpad
            self.path("ssleay32.pdb")
            self.path("libeay32.pdb")
            #self.path("growl.pdb")
            #self.path("growl++.pdb")
            self.path('apr-1.pdb', 'libarp.pdb')
            self.path('aprutil-1.pdb', 'libaprutil.pdb')
            # </FS:ND>

            # For google-perftools tcmalloc allocator.
            try:
                if self.args['configuration'].lower() == 'debug':
                    self.path('libtcmalloc_minimal-debug.dll')
                else:
                    self.path('libtcmalloc_minimal.dll')
            except:
                print "Skipping libtcmalloc_minimal.dll"

        self.path(src="licenses-win32.txt", dst="licenses.txt")
        self.path("featuretable.txt")

        with self.prefix(src=pkgdir):
            self.path("ca-bundle.crt")
        self.path("VivoxAUP.txt")

        # Media plugins - CEF
        with self.prefix(dst="llplugin"):
            with self.prefix(src=os.path.join(self.args['build'], os.pardir, 'media_plugins')):
                with self.prefix(src=os.path.join('cef', self.args['configuration'])):
                    self.path("media_plugin_cef.dll")

                # Media plugins - LibVLC
                with self.prefix(src=os.path.join('libvlc', self.args['configuration'])):
                    self.path("media_plugin_libvlc.dll")

                # Media plugins - Example (useful for debugging - not shipped with release viewer)
                if self.channel_type() != 'release':
                    with self.prefix(src=os.path.join('example', self.args['configuration'])):
                        self.path("media_plugin_example.dll")

            # CEF runtime files - debug
            # CEF runtime files - not debug (release, relwithdebinfo etc.)
            config = 'debug' if self.args['configuration'].lower() == 'debug' else 'release'
            with self.prefix(src=os.path.join(pkgdir, 'bin', config)):
                self.path("chrome_elf.dll")
                self.path("d3dcompiler_43.dll")
                self.path("d3dcompiler_47.dll")
                self.path("libcef.dll")
                self.path("libEGL.dll")
                self.path("libGLESv2.dll")
                self.path("dullahan_host.exe")
                self.path("natives_blob.bin")
                self.path("snapshot_blob.bin")
                self.path("widevinecdmadapter.dll")

            # MSVC DLLs needed for CEF and have to be in same directory as plugin
            with self.prefix(src=os.path.join(self.args['build'], os.pardir,
                                              'sharedlibs', 'Release')):
                self.path("msvcp120.dll")
                self.path("msvcr120.dll")

            # CEF files common to all configurations
            with self.prefix(src=os.path.join(pkgdir, 'resources')):
                self.path("cef.pak")
                self.path("cef_100_percent.pak")
                self.path("cef_200_percent.pak")
                self.path("cef_extensions.pak")
                self.path("devtools_resources.pak")
                self.path("icudtl.dat")

            with self.prefix(src=os.path.join(pkgdir, 'resources', 'locales'), dst='locales'):
                self.path("am.pak")
                self.path("ar.pak")
                self.path("bg.pak")
                self.path("bn.pak")
                self.path("ca.pak")
                self.path("cs.pak")
                self.path("da.pak")
                self.path("de.pak")
                self.path("el.pak")
                self.path("en-GB.pak")
                self.path("en-US.pak")
                self.path("es-419.pak")
                self.path("es.pak")
                self.path("et.pak")
                self.path("fa.pak")
                self.path("fi.pak")
                self.path("fil.pak")
                self.path("fr.pak")
                self.path("gu.pak")
                self.path("he.pak")
                self.path("hi.pak")
                self.path("hr.pak")
                self.path("hu.pak")
                self.path("id.pak")
                self.path("it.pak")
                self.path("ja.pak")
                self.path("kn.pak")
                self.path("ko.pak")
                self.path("lt.pak")
                self.path("lv.pak")
                self.path("ml.pak")
                self.path("mr.pak")
                self.path("ms.pak")
                self.path("nb.pak")
                self.path("nl.pak")
                self.path("pl.pak")
                self.path("pt-BR.pak")
                self.path("pt-PT.pak")
                self.path("ro.pak")
                self.path("ru.pak")
                self.path("sk.pak")
                self.path("sl.pak")
                self.path("sr.pak")
                self.path("sv.pak")
                self.path("sw.pak")
                self.path("ta.pak")
                self.path("te.pak")
                self.path("th.pak")
                self.path("tr.pak")
                self.path("uk.pak")
                self.path("vi.pak")
                self.path("zh-CN.pak")
                self.path("zh-TW.pak")

            with self.prefix(src=os.path.join(pkgdir, 'bin', 'release')):
                self.path("libvlc.dll")
                self.path("libvlccore.dll")
                self.path("plugins/")

        # pull in the crash logger and updater from other projects
        # tag:"crash-logger" here as a cue to the exporter
        self.path(src='../win_crash_logger/%s/windows-crash-logger.exe' % self.args['configuration'],
                  dst="win_crash_logger.exe")

        # <FS:Ansariel> This is still needed! The method to copy the Visual C++ Runtime files
        #               in Copy3rdPartyLibs is copying the wrong files for 64bit because Autobuild
        #               is a 32bit process and Windows will silently copy the 32bit versions from
        #               the SysWOW64 folder, even if explicitly trying to copy from System32!
        if (self.address_size == 64):
            with self.prefix(src=os.path.join(self.args['build'], os.pardir, os.pardir, 'indra', 'newview', 'installers', 'windows_x64'), dst="llplugin"):
                self.path("msvcp120.dll")
                self.path("msvcr120.dll")
            with self.prefix(src=os.path.join(self.args['build'], os.pardir, os.pardir, 'indra', 'newview', 'installers', 'windows_x64')):
                self.path("msvcp120.dll")
                self.path("msvcr120.dll")

        # <FS:Ansariel> FIRE-22709: Local voice not working in OpenSim
        if self.fs_is_opensim():
            with self.prefix(src=os.path.join(relpkgdir, 'voice_os'), dst="voice_os"):
                self.path("libsndfile-1.dll")
                self.path("ortp.dll")
                self.path("SLVoice.exe")
                self.path("vivoxoal.dll")
                self.path("vivoxsdk.dll")
            with self.prefix(src=pkgdir, dst="voice_os"):
                self.path("ca-bundle.crt")
        # </FS:Ansariel>

        if not self.is_packaging_viewer():
            self.package_file = "copied_deps"    

        self.fs_copy_windows_manifest( )

    def nsi_file_commands(self, install=True):
        def wpath(path):
            if path.endswith('/') or path.endswith(os.path.sep):
                path = path[:-1]
            path = path.replace('/', '\\')
            return path

        result = ""
        dest_files = [pair[1] for pair in self.file_list if pair[0] and os.path.isfile(pair[1]) and not pair[1].endswith(".pdb") ] #<FS:ND/> Don't include pdb files.
        # sort deepest hierarchy first
        dest_files.sort(lambda a,b: cmp(a.count(os.path.sep),b.count(os.path.sep)) or cmp(a,b))
        dest_files.reverse()
        out_path = None
        for pkg_file in dest_files:
            rel_file = os.path.normpath(pkg_file.replace(self.get_dst_prefix()+os.path.sep,''))
            installed_dir = wpath(os.path.join('$INSTDIR', os.path.dirname(rel_file)))
            pkg_file = wpath(os.path.normpath(pkg_file))
            if installed_dir != out_path:
                if install:
                    out_path = installed_dir
                    result += 'SetOutPath ' + out_path + '\n'
            if install:
                result += 'File ' + pkg_file + '\n'
            else:
                result += 'Delete ' + wpath(os.path.join('$INSTDIR', rel_file)) + '\n'

        # at the end of a delete, just rmdir all the directories
        if not install:
            deleted_file_dirs = [os.path.dirname(pair[1].replace(self.get_dst_prefix()+os.path.sep,'')) for pair in self.file_list]
            # find all ancestors so that we don't skip any dirs that happened to have no non-dir children
            deleted_dirs = []
            for d in deleted_file_dirs:
                deleted_dirs.extend(path_ancestors(d))
            # sort deepest hierarchy first
            deleted_dirs.sort(lambda a,b: cmp(a.count(os.path.sep),b.count(os.path.sep)) or cmp(a,b))
            deleted_dirs.reverse()
            prev = None
            for d in deleted_dirs:
                if d != prev:   # skip duplicates
                    result += 'RMDir ' + wpath(os.path.join('$INSTDIR', os.path.normpath(d))) + '\n'
                prev = d

        return result

    def package_finish(self):
        # a standard map of strings for replacing in the templates
        substitution_strings = {
            'version' : '.'.join(self.args['version']),
            'version_short' : '.'.join(self.args['version'][:-1]),
            'version_dashes' : '-'.join(self.args['version']),
            'version_registry' : '%s(%s)' %
            ('.'.join(self.args['version']), self.address_size),
            'final_exe' : self.final_exe(),
            'flags':'',
            'app_name':self.app_name(),
            'app_name_oneword':self.app_name_oneword()
            }

        substitution_strings = self.fs_splice_grid_substitution_strings( substitution_strings ) #<FS:ND/> Add grid args

        # <FS:ND> Properly name OS version, also add Phoenix- in front of installer name
        #installer_file = self.installer_base_name() + '_Setup.exe'
        installer_file = "Phoenix-%(app_name)s-%(version_dashes)s_Setup.exe" % substitution_strings
        # </FS:ND>
        
        substitution_strings['installer_file'] = installer_file
        substitution_strings['is64bit'] = (1 if (self.address_size == 64) else 0)

        version_vars = """
        !define INSTEXE "SL_Launcher.exe"
        !define VERSION "%(version_short)s"
        !define VERSION_LONG "%(version)s"
        !define VERSION_DASHES "%(version_dashes)s"
        !define VERSION_REGISTRY "%(version_registry)s"
        !define VIEWER_EXE "%(final_exe)s"
        """ % substitution_strings
        
        if self.channel_type() == 'release':
            substitution_strings['caption'] = CHANNEL_VENDOR_BASE
        else:
            substitution_strings['caption'] = self.app_name() + ' ${VERSION}'

        inst_vars_template = """
            OutFile "%(installer_file)s"
            !define INSTNAME   "%(app_name_oneword)s"
            !define SHORTCUT   "%(app_name)s"
            !define URLNAME   "secondlife"
            !define IS64BIT   "%(is64bit)d"
            Caption "%(caption)s"
            """

        if(self.address_size == 64):
            engage_registry="SetRegView 64"
            program_files="$PROGRAMFILES64"
        else:
            engage_registry="SetRegView 32"
            program_files="$PROGRAMFILES32"

        tempfile = "firestorm_setup_tmp.nsi"

        self.fs_sign_win_binaries() # <FS:ND/> Sign files, step one. Sign compiled binaries

        # the following replaces strings in the nsi template
        # it also does python-style % substitution
        self.replace_in("installers/windows/installer_template.nsi", tempfile, {
                "%%VERSION%%":version_vars,
                "%%SOURCE%%":self.get_src_prefix(),
                "%%INST_VARS%%":inst_vars_template % substitution_strings,
                "%%INSTALL_FILES%%":self.nsi_file_commands(True),
                "%%PROGRAMFILES%%":program_files,
                "%%ENGAGEREGISTRY%%":engage_registry,
                "%%DELETE_FILES%%":self.nsi_file_commands(False)})

        # If we're on a build machine, sign the code using our Authenticode certificate. JC
        # note that the enclosing setup exe is signed later, after the makensis makes it.
        # Unlike the viewer binary, the VMP filenames are invariant with respect to version, os, etc.
        #for exe in (
        #    "SL_Launcher.exe",
        #    ):
        #    self.sign(exe)
            
        # We use the Unicode version of NSIS, available from
        # http://www.scratchpaper.com/
        # Check two paths, one for Program Files, and one for Program Files (x86).
        # Yay 64bit windows.
        for ProgramFiles in 'ProgramFiles', 'ProgramFiles(x86)':
            NSIS_path = os.path.expandvars(r'${%s}\NSIS\Unicode\makensis.exe' % ProgramFiles)
            if os.path.exists(NSIS_path):
                break
        installer_created=False
        nsis_attempts=3
        nsis_retry_wait=15
        for attempt in xrange(nsis_attempts):
            try:
                self.run_command([NSIS_path, '/V2', self.dst_path_of(tempfile)])
            except ManifestError as err:
                if attempt+1 < nsis_attempts:
                    print >> sys.stderr, "nsis failed, waiting %d seconds before retrying" % nsis_retry_wait
                    time.sleep(nsis_retry_wait)
                    nsis_retry_wait*=2
            else:
                # NSIS worked! Done!
                break
        else:
            print >> sys.stderr, "Maximum nsis attempts exceeded; giving up"
            raise

        self.fs_sign_win_installer(substitution_strings) # <FS:ND/> Sign files, step two. Sign installer.
        self.fs_save_windows_symbols()

        self.created_path(self.dst_path_of(installer_file))
        self.package_file = installer_file

    def sign(self, exe):
        sign_py = os.environ.get('SIGN', r'C:\buildscripts\code-signing\sign.py')
        python  = os.environ.get('PYTHON', 'python')
        if os.path.exists(sign_py):
            dst_path = self.dst_path_of(exe)
            print "about to run signing of: ", dst_path
            self.run_command([python, sign_py, dst_path])
        else:
            print "Skipping code signing of %s: %s not found" % (exe, sign_py)

    def escape_slashes(self, path):
        return path.replace('\\', '\\\\\\\\')

class Windows_i686_Manifest(WindowsManifest):
    # Although we aren't literally passed ADDRESS_SIZE, we can infer it from
    # the passed 'arch', which is used to select the specific subclass.
    address_size = 32

class Windows_x86_64_Manifest(WindowsManifest):
    address_size = 64


class DarwinManifest(ViewerManifest):
    build_data_json_platform = 'mac'

    def finish_build_data_dict(self, build_data_dict):
        build_data_dict.update({'Bundle Id':self.args['bundleid']})
        return build_data_dict

    def is_packaging_viewer(self):
        # darwin requires full app bundle packaging even for debugging.
        return True

# <FS:Ansariel> construct method VMP trampoline crazy VMP launcher juggling shamelessly replaced with old version
    # def construct(self):
        # # These are the names of the top-level application and the embedded
        # # applications for the VMP and for the actual viewer, respectively.
        # # These names, without the .app suffix, determine the flyover text for
        # # their corresponding Dock icons.
        # toplevel_app, toplevel_icon = "Second Life.app",          "secondlife.icns"
        # launcher_app, launcher_icon = "Second Life Launcher.app", "secondlife.icns"
        # viewer_app,   viewer_icon   = "Second Life Viewer.app",   "secondlife.icns"

        # # capture the path to the directory containing toplevel_app
        # parentdir = os.path.join(self.get_dst_prefix(), os.pardir)

        # # copy over the build result (this is a no-op if run within the xcode script)
        # self.path(os.path.join(self.args['configuration'], toplevel_app), dst="")

        # pkgdir = os.path.join(self.args['build'], os.pardir, 'packages')
        # relpkgdir = os.path.join(pkgdir, "lib", "release")
        # debpkgdir = os.path.join(pkgdir, "lib", "debug")

        # # -------------------- top-level Second Life.app ---------------------
        # # top-level Second Life application is only a container
        # with self.prefix(dst="Contents"):  # everything goes in Contents
            # # top-level Info.plist is as generated by CMake
            # Info_plist = self.dst_path_of("Info.plist")

            # # the one file in top-level MacOS directory is the trampoline to
            # # our nested launcher_app
            # with self.prefix(dst="MacOS"):
                # toplevel_MacOS = self.get_dst_prefix()
                # trampoline = self.put_in_file("""\
# #!/bin/bash
# open "%s" --args "$@"
# """ %
                    # # up one directory from MacOS to its sibling Resources directory
                    # os.path.join('$(dirname "$0")', os.pardir, 'Resources', launcher_app),
                    # "SL_Launcher",      # write this file
                    # "trampoline")       # flag to add to list of copied files
                # # Script must be executable
                # self.run_command(["chmod", "+x", trampoline])

            # # Make a symlink to a nested app Frameworks directory that doesn't
            # # yet exist. We shouldn't need this; the only things that need
            # # Frameworks are nested apps under viewer_app, and they should
            # # simply find its Contents/Frameworks by relative pathnames. But
            # # empirically, we do: if we omit this symlink, CEF doesn't work --
            # # the login splash screen doesn't even display. SIIIIGH.
            # # We're passing a path that's already relative, hence symlinkf()
            # # rather than relsymlinkf().
            # self.symlinkf(os.path.join("Resources", viewer_app, "Contents", "Frameworks"))

            # with self.prefix(dst="Resources"):
                # # top-level Resources directory should be pretty sparse
                # # need .icns file referenced by top-level Info.plist
                # with self.prefix(src=self.icon_path()) :
                    # self.path(toplevel_icon)

                # # ------------------- nested launcher_app --------------------
                # with self.prefix(dst=os.path.join(launcher_app, "Contents")):
                    # # Info.plist is just like top-level one...
                    # Info = plistlib.readPlist(Info_plist)
                    # # except for these replacements:
                    # Info["CFBundleExecutable"] = "SL_Launcher"
                    # Info["CFBundleIconFile"] = launcher_icon
                    # self.put_in_file(
                        # plistlib.writePlistToString(Info),
                        # os.path.basename(Info_plist),
                        # "Info.plist")

                    # # copy VMP libs to MacOS
                    # with self.prefix(dst="MacOS"):              
                        # #this copies over the python wrapper script,
                        # #associated utilities and required libraries, see
                        # #SL-321, SL-322, SL-323
                        # with self.prefix(src=os.path.join(pkgdir, "VMP")):
                            # self.path("SL_Launcher")
                            # self.path("*.py")
                            # # certifi will be imported by requests; this is
                            # # our custom version to get our ca-bundle.crt
                            # self.path("certifi")
                        # with self.prefix(src=os.path.join(pkgdir, "lib", "python")):
                            # # llbase provides our llrest service layer and llsd decoding
                            # with self.prefix(src="llbase", dst="llbase"):
                                # # (Why is llbase treated specially here? What
                                # # DON'T we want to copy out of lib/python/llbase?)
                                # self.path("*.py")
                                # self.path("_cllsd.so")
                            # #requests module needed by llbase/llrest.py
                            # #this is only needed on POSIX, because in Windows
                            # #we compile it into the EXE
                            # for pypkg in "chardet", "idna", "requests", "urllib3":
                                # self.path(pypkg)

                    # # launcher_app/Contents/Resources
                    # with self.prefix(dst="Resources"):
                        # with self.prefix(src=self.icon_path()) :
                            # self.path(launcher_icon)
                            # with self.prefix(dst="vmp_icons"):
                                # self.path("secondlife.ico")
                        # #VMP Tkinter icons
                        # with self.prefix(src_dst="vmp_icons"):
                            # self.path("*.png")
                            # self.path("*.gif")

                # # -------------------- nested viewer_app ---------------------
                # with self.prefix(dst=os.path.join(viewer_app, "Contents")):
                    # defer Info.plist until after MacOS
                    # with self.prefix(dst="MacOS"):
                        # # CMake constructs the Second Life executable in the
                        # # MacOS directory belonging to the top-level Second
                        # # Life.app. Move it here.
                        # here = self.get_dst_prefix()
                        # relbase = os.path.realpath(os.path.dirname(Info_plist))
                        # self.cmakedirs(here)
                        # for f in os.listdir(toplevel_MacOS):
                            # if f == os.path.basename(trampoline):
                                # # don't move the trampoline script we just made!
                                # continue
                            # fromwhere = os.path.join(toplevel_MacOS, f)
                            # towhere   = self.dst_path_of(f)
                            # print "Moving %s => %s" % \
                                  # (self.relpath(fromwhere, relbase),
                                   # self.relpath(towhere, relbase))
                            # # now do it, only without relativizing paths
                            # os.rename(fromwhere, towhere)

                        # Pick the biggest of the executables as the real viewer.
                        # Make (basename, fullpath) pairs; for each pair,
                        # expand to (size, basename, fullpath) triples; sort
                        # by size; pick the last triple; take the basename and
                        # fullpath from that.
                        # _, exename, exepath = \
                        #     sorted((os.path.getsize(path), name, path)
                        #            for name, path in
                        #            ((name, os.path.join(here, name))
                        #             for name in os.listdir(here)))[-1]

                        # if ("package" in self.args['actions'] or 
                        #     "unpacked" in self.args['actions']):
                            # # NOTE: the -S argument to strip causes it to keep
                            # # enough info for annotated backtraces (i.e. function
                            # # names in the crash log). 'strip' with no arguments
                            # # yields a slightly smaller binary but makes crash
                            # # logs mostly useless. This may be desirable for the
                            # # final release. Or not.
                            # self.run_command(['strip', '-S', exepath])

                    # # Info.plist is just like top-level one...
                    # Info = plistlib.readPlist(Info_plist)
                    # # except for these replacements:
                    # # (CFBundleExecutable may be moot: SL_Launcher directly
                    # # runs the executable, instead of launching the app)
                    # Info["CFBundleExecutable"] = exename
                    # Info["CFBundleIconFile"] = viewer_icon
                    # bugsplat_db = self.args.get('bugsplat')
                    # if bugsplat_db:
                        # # https://www.bugsplat.com/docs/platforms/os-x#configuration
                        # Info["BugsplatServerURL"] = \
                            # "https://{}.bugsplat.com/".format(bugsplat_db)
                    # self.put_in_file(
                        # plistlib.writePlistToString(Info),
                        # os.path.basename(Info_plist),
                        # "Info.plist")

                    # with self.prefix(dst="Frameworks"):
                        # # CEF framework goes inside viewer_app/Contents/Frameworks.
                        # CEF_framework = "Chromium Embedded Framework.framework"
                        # self.path2basename(relpkgdir, CEF_framework)
                        # # Remember where we parked this car.
                        # CEF_framework = self.dst_path_of(CEF_framework)

                        # if self.args.get('bugsplat'):
                            # self.path2basename(relpkgdir, "BugsplatMac.framework")

                    # with self.prefix(dst="Resources"):
                        # # defer cross-platform file copies until we're in the right
                        # # nested Resources directory
                        # super(DarwinManifest, self).construct()

                        # with self.prefix(src=self.icon_path()) :
                            # self.path(viewer_icon)

                        # with self.prefix(src=relpkgdir):
                            # self.path("libndofdev.dylib")
                            # self.path("libhunspell-1.3.0.dylib")   

                        # with self.prefix("cursors_mac"):
                            # self.path("*.tif")

                        # self.path("licenses-mac.txt", dst="licenses.txt")
                        # self.path("featuretable_mac.txt")
                        # self.path("SecondLife.nib")

                        # with self.prefix(src=pkgdir,dst=""):
                            # self.path("ca-bundle.crt")

                        # self.path("SecondLife.nib")

                        # # Translations
                        # self.path("English.lproj/language.txt")
                        # self.replace_in(src="English.lproj/InfoPlist.strings",
                                        # dst="English.lproj/InfoPlist.strings",
                                        # searchdict={'%%VERSION%%':'.'.join(self.args['version'])}
                                        # )
                        # self.path("German.lproj")
                        # self.path("Japanese.lproj")
                        # self.path("Korean.lproj")
                        # self.path("da.lproj")
                        # self.path("es.lproj")
                        # self.path("fr.lproj")
                        # self.path("hu.lproj")
                        # self.path("it.lproj")
                        # self.path("nl.lproj")
                        # self.path("pl.lproj")
                        # self.path("pt.lproj")
                        # self.path("ru.lproj")
                        # self.path("tr.lproj")
                        # self.path("uk.lproj")
                        # self.path("zh-Hans.lproj")

                        # def path_optional(src, dst):
                            # """
                            # For a number of our self.path() calls, not only do we want
                            # to deal with the absence of src, we also want to remember
                            # which were present. Return either an empty list (absent)
                            # or a list containing dst (present). Concatenate these
                            # return values to get a list of all libs that are present.
                            # """
                            # # This was simple before we started needing to pass
                            # # wildcards. Fortunately, self.path() ends up appending a
                            # # (source, dest) pair to self.file_list for every expanded
                            # # file processed. Remember its size before the call.
                            # oldlen = len(self.file_list)
                            # self.path(src, dst)
                            # # The dest appended to self.file_list has been prepended
                            # # with self.get_dst_prefix(). Strip it off again.
                            # added = [os.path.relpath(d, self.get_dst_prefix())
                                     # for s, d in self.file_list[oldlen:]]
                            # if not added:
                                # print "Skipping %s" % dst
                            # return added

                        # # dylibs is a list of all the .dylib files we expect to need
                        # # in our bundled sub-apps. For each of these we'll create a
                        # # symlink from sub-app/Contents/Resources to the real .dylib.
                        # # Need to get the llcommon dll from any of the build directories as well.
                        # libfile_parent = self.get_dst_prefix()
                        # libfile = "libllcommon.dylib"
                        # dylibs = path_optional(self.find_existing_file(os.path.join(os.pardir,
                                                                       # "llcommon",
                                                                       # self.args['configuration'],
                                                                       # libfile),
                                                                       # os.path.join(relpkgdir, libfile)),
                                               # dst=libfile)

                        # for libfile in (
                                        # "libapr-1.0.dylib",
                                        # "libaprutil-1.0.dylib",
                                        # "libcollada14dom.dylib",
                                        # "libexpat.1.dylib",
                                        # "libexception_handler.dylib",
                                        # "libGLOD.dylib",
                                        # # libnghttp2.dylib is a symlink to
                                        # # libnghttp2.major.dylib, which is a symlink to
                                        # # libnghttp2.version.dylib. Get all of them.
                                        # "libnghttp2.*dylib",
                                        # ):
                            # dylibs += path_optional(os.path.join(relpkgdir, libfile), libfile)

                        # # SLVoice and vivox lols, no symlinks needed
                        # for libfile in (
                                        # 'libortp.dylib',
                                        # 'libsndfile.dylib',
                                        # 'libvivoxoal.dylib',
                                        # 'libvivoxsdk.dylib',
                                        # 'libvivoxplatform.dylib',
                                        # 'SLVoice',
                                        # ):
                            # self.path2basename(relpkgdir, libfile)

                        # # dylibs that vary based on configuration
                        # if self.args['configuration'].lower() == 'debug':
                            # for libfile in (
                                        # "libfmodexL.dylib",
                                        # ):
                                # dylibs += path_optional(os.path.join(debpkgdir, libfile), libfile)
                        # else:
                            # for libfile in (
                                        # "libfmodex.dylib",
                                        # ):
                                # dylibs += path_optional(os.path.join(relpkgdir, libfile), libfile)

                        # # our apps
                        # executable_path = {}
                        # for app_bld_dir, app in (("mac_crash_logger", "mac-crash-logger.app"),
                                                 # # plugin launcher
                                                 # (os.path.join("llplugin", "slplugin"), "SLPlugin.app"),
                                                 # ):
                            # self.path2basename(os.path.join(os.pardir,
                                                            # app_bld_dir, self.args['configuration']),
                                               # app)
                            # executable_path[app] = \
                                # self.dst_path_of(os.path.join(app, "Contents", "MacOS"))

                            # # our apps dependencies on shared libs
                            # # for each app, for each dylib we collected in dylibs,
                            # # create a symlink to the real copy of the dylib.
                            # with self.prefix(dst=os.path.join(app, "Contents", "Resources")):
                                # for libfile in dylibs:
                                    # self.relsymlinkf(os.path.join(libfile_parent, libfile))

                        # # Dullahan helper apps go inside SLPlugin.app
                        # with self.prefix(dst=os.path.join(
                            # "SLPlugin.app", "Contents", "Frameworks")):

                            # frameworkname = 'Chromium Embedded Framework'

                            # # This code constructs a relative symlink from the
                            # # target framework folder back to the real CEF framework.
                            # # It needs to be relative so that the symlink still works when
                            # # (as is normal) the user moves the app bundle out of the DMG
                            # # and into the /Applications folder. Note we pass catch=False,
                            # # letting the uncaught exception terminate the process, since
                            # # without this symlink, Second Life web media can't possibly work.

                            # # It might seem simpler just to symlink Frameworks back to
                            # # the parent of Chromimum Embedded Framework.framework. But
                            # # that would create a symlink cycle, which breaks our
                            # # packaging step. So make a symlink from Chromium Embedded
                            # # Framework.framework to the directory of the same name, which
                            # # is NOT an ancestor of the symlink.

                            # # from SLPlugin.app/Contents/Frameworks/Chromium Embedded
                            # # Framework.framework back to
                            # # $viewer_app/Contents/Frameworks/Chromium Embedded Framework.framework
                            # SLPlugin_framework = self.relsymlinkf(CEF_framework, catch=False)

                            # # copy DullahanHelper.app
                            # self.path2basename(relpkgdir, 'DullahanHelper.app')

                            # # and fix that up with a Frameworks/CEF symlink too
                            # with self.prefix(dst=os.path.join(
                                # 'DullahanHelper.app', 'Contents', 'Frameworks')):
                                # # from Dullahan Helper.app/Contents/Frameworks/Chromium Embedded
                                # # Framework.framework back to
                                # # SLPlugin.app/Contents/Frameworks/Chromium Embedded Framework.framework
                                # # Since SLPlugin_framework is itself a
                                # # symlink, don't let relsymlinkf() resolve --
                                # # explicitly call relpath(symlink=True) and
                                # # create that symlink here.
                                # DullahanHelper_framework = \
                                    # self.symlinkf(self.relpath(SLPlugin_framework, symlink=True),
                                                  # catch=False)

                            # # change_command includes install_name_tool, the
                            # # -change subcommand and the old framework rpath
                            # # stamped into the executable. To use it with
                            # # run_command(), we must still append the new
                            # # framework path and the pathname of the
                            # # executable to change.
                            # change_command = [
                                # 'install_name_tool', '-change',
                                # '@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework']

                            # with self.prefix(dst=os.path.join(
                                # 'DullahanHelper.app', 'Contents', 'MacOS')):
                                # # Now self.get_dst_prefix() is, at runtime,
                                # # @executable_path. Locate the helper app
                                # # framework (which is a symlink) from here.
                                # newpath = os.path.join(
                                    # '@executable_path',
                                    # self.relpath(DullahanHelper_framework, symlink=True),
                                    # frameworkname)
                                # # and restamp the DullahanHelper executable
                                # self.run_command(
                                    # change_command +
                                    # [newpath, self.dst_path_of('DullahanHelper')])

                        # # SLPlugin plugins
                        # with self.prefix(dst="llplugin"):
                            # dylibexecutable = 'media_plugin_cef.dylib'
                            # self.path2basename("../media_plugins/cef/" + self.args['configuration'],
                                               # dylibexecutable)

                            # # Do this install_name_tool *after* media plugin is copied over.
                            # # Locate the framework lib executable -- relative to
                            # # SLPlugin.app/Contents/MacOS, which will be our
                            # # @executable_path at runtime!
                            # newpath = os.path.join(
                                # '@executable_path',
                                # self.relpath(SLPlugin_framework, executable_path["SLPlugin.app"],
                                             # symlink=True),
                                # frameworkname)
                            # # restamp media_plugin_cef.dylib
                            # self.run_command(
                                # change_command +
                                # [newpath, self.dst_path_of(dylibexecutable)])

                            # # copy LibVLC plugin itself
                            # self.path2basename("../media_plugins/libvlc/" + self.args['configuration'],
                                               # "media_plugin_libvlc.dylib")

                            # # copy LibVLC dynamic libraries
                            # with self.prefix(src=relpkgdir, dst="lib"):
                                # self.path( "libvlc*.dylib*" )
                                # # copy LibVLC plugins folder
                                # with self.prefix(src='plugins', dst=""):
                                    # self.path( "*.dylib" )
                                    # self.path( "plugins.dat" )
    def construct(self):
        # copy over the build result (this is a no-op if run within the xcode script)
        self.path(self.args['configuration'] + "/Firestorm.app", dst="")

        pkgdir = os.path.join(self.args['build'], os.pardir, 'packages')
        relpkgdir = os.path.join(pkgdir, "lib", "release")
        debpkgdir = os.path.join(pkgdir, "lib", "debug")
        requestsdir = os.path.join(pkgdir, "lib", "python", "requests")
        urllib3dir = os.path.join(pkgdir, "lib", "python", "urllib3")
        chardetdir = os.path.join(pkgdir, "lib", "python", "chardet")
        idnadir = os.path.join(pkgdir, "lib", "python", "idna")

        with self.prefix(dst="Contents"):  # everything goes in Contents
            self.path("Info.plist", dst="Info.plist")

            # copy additional libs in <bundle>/Contents/MacOS/
            self.path(os.path.join(relpkgdir, "libndofdev.dylib"), dst="Resources/libndofdev.dylib")
            self.path(os.path.join(relpkgdir, "libhunspell-1.3.0.dylib"), dst="Resources/libhunspell-1.3.0.dylib")   

            # Growl Frameworks
            self.path("../packages/Frameworks/Growl", dst="Frameworks/Growl")

            # most everything goes in the Resources directory
            with self.prefix(dst="Resources"):
                super(DarwinManifest, self).construct()

                with self.prefix(src_dst="cursors_mac"):
                    self.path("*.tif")

                self.path("licenses-mac.txt", dst="licenses.txt")
                self.path("featuretable_mac.txt")
                self.path("VivoxAUP.txt")

                with self.prefix(src=pkgdir):
                    self.path("ca-bundle.crt")

                icon_path = self.icon_path()
                with self.prefix(src=icon_path) :
                    self.path("firestorm_icon.icns")

                self.path("Firestorm.nib")
                
                # Translations
                self.path("English.lproj/language.txt")
                self.replace_in(src="English.lproj/InfoPlist.strings",
                                dst="English.lproj/InfoPlist.strings",
                                searchdict={'%%VERSION%%':'.'.join(self.args['version'])}
                                )
                self.path("German.lproj")
                self.path("Japanese.lproj")
                self.path("Korean.lproj")
                self.path("da.lproj")
                self.path("es.lproj")
                self.path("fr.lproj")
                self.path("hu.lproj")
                self.path("it.lproj")
                self.path("nl.lproj")
                self.path("pl.lproj")
                self.path("pt.lproj")
                self.path("ru.lproj")
                self.path("tr.lproj")
                self.path("uk.lproj")
                self.path("zh-Hans.lproj")

                def path_optional(src, dst):
                    """
                    For a number of our self.path() calls, not only do we want
                    to deal with the absence of src, we also want to remember
                    which were present. Return either an empty list (absent)
                    or a list containing dst (present). Concatenate these
                    return values to get a list of all libs that are present.
                    """
                    # This was simple before we started needing to pass
                    # wildcards. Fortunately, self.path() ends up appending a
                    # (source, dest) pair to self.file_list for every expanded
                    # file processed. Remember its size before the call.
                    oldlen = len(self.file_list)
                    self.path(src, dst)
                    # The dest appended to self.file_list has been prepended
                    # with self.get_dst_prefix(). Strip it off again.
                    added = [os.path.relpath(d, self.get_dst_prefix())
                             for s, d in self.file_list[oldlen:]]
                    if not added:
                        print "Skipping %s" % dst
                    return added

                # dylibs is a list of all the .dylib files we expect to need
                # in our bundled sub-apps. For each of these we'll create a
                # symlink from sub-app/Contents/Resources to the real .dylib.
                # Need to get the llcommon dll from any of the build directories as well.
                libfile = "libllcommon.dylib"
                dylibs = path_optional(self.find_existing_file(os.path.join(os.pardir,
                                                               "llcommon",
                                                               self.args['configuration'],
                                                               libfile),
                                                               os.path.join(relpkgdir, libfile)),
                                       dst=libfile)

                for libfile in (
                                "libapr-1.0.dylib",
                                "libaprutil-1.0.dylib",
                                "libcollada14dom.dylib",
                                "libexpat.1.dylib",
                                "libexception_handler.dylib",
                                "libGLOD.dylib",
                                # libnghttp2.dylib is a symlink to
                                # libnghttp2.major.dylib, which is a symlink
                                # to libnghttp2.version.dylib. Get all of them.
                                "libnghttp2.*dylib",
                                "libgrowl.dylib",
                                "libgrowl++.dylib",
                                "libLeap.dylib",
                                ):
                    dylibs += path_optional(os.path.join(relpkgdir, libfile), libfile)

                # SLVoice and vivox lols, no symlinks needed
                for libfile in (
                                'libalut.dylib',
                                'libopenal.dylib',
                                'libortp.dylib',
                                'libsndfile.dylib',
                                'libvivoxoal.dylib',
                                'libvivoxsdk.dylib',
                                'libvivoxplatform.dylib',
                                'SLVoice',
                                ):
                    self.path2basename(relpkgdir, libfile)

                # <FS:Ansariel/TS> FIRE-22709: Local voice not working in OpenSim
                if self.fs_is_opensim():
                    with self.prefix(src=os.path.join(relpkgdir, 'voice_os'), dst="voice_os"):
                        self.path('libalut.dylib')
                        self.path('libopenal.dylib')
                        self.path('libortp.dylib')
                        self.path('libsndfile.dylib')
                        self.path('libvivoxoal.dylib')
                        self.path('libvivoxsdk.dylib')
                        self.path('libvivoxplatform.dylib')
                        self.path('SLVoice')
                    with self.prefix(src=pkgdir, dst="voice_os"):
                        self.path("ca-bundle.crt")
                # </FS:Ansariel/TS>

                # dylibs that vary based on configuration
                if self.args['configuration'].lower() == 'debug':
                    for libfile in (
                                "libfmodL.dylib",
                                ):
                        dylibs += path_optional(os.path.join(debpkgdir, libfile), libfile)
                else:
                    for libfile in (
                                "libfmod.dylib",
                                ):
                        dylibs += path_optional(os.path.join(relpkgdir, libfile), libfile)

                # dylibs that vary based on configuration
                if self.args['configuration'].lower() == 'debug':
                    for libfile in (
                                "libfmodexL.dylib",
                                ):
                        dylibs += path_optional(os.path.join(debpkgdir, libfile), libfile)
                else:
                    for libfile in (
                                "libfmodex.dylib",
                                ):
                        dylibs += path_optional(os.path.join(relpkgdir, libfile), libfile)
                
                # our apps
                for app_bld_dir, app in (("mac_crash_logger", "mac-crash-logger.app"),
                                         # plugin launcher
                                         (os.path.join("llplugin", "slplugin"), "SLPlugin.app"),
                                         ):
                    self.path2basename(os.path.join(os.pardir,
                                                    app_bld_dir, self.args['configuration']),
                                       app)

                    # our apps dependencies on shared libs
                    # for each app, for each dylib we collected in dylibs,
                    # create a symlink to the real copy of the dylib.
                    resource_path = self.dst_path_of(os.path.join(app, "Contents", "Resources"))
                    for libfile in dylibs:
                        src = os.path.join(os.pardir, os.pardir, os.pardir, libfile)
                        dst = os.path.join(resource_path, libfile)
                        try:
                            symlinkf(src, dst)
                        except OSError as err:
                            print "Can't symlink %s -> %s: %s" % (src, dst, err)

                #<FS:TS> Moved from the x86_64 specific version because code
                # below that does symlinking and path fixup depends on it.
                with self.prefix(src=os.path.join(self.args['build'], os.pardir, 'packages', 'bin_x86')):
                    self.path("SLPlugin.app", "SLPlugin.app")
	
                    with self.prefix(src_dst="llplugin"):
                        self.path("media_plugin_quicktime.dylib", "media_plugin_quicktime.dylib")
                        self.path("media_plugin_cef.dylib", "media_plugin_cef.dylib")

                # Dullahan helper apps go inside SLPlugin.app
                with self.prefix(dst="SLPlugin.app/Contents/Frameworks"):
                    helperappfile = 'DullahanHelper.app'
                    self.path2basename(relpkgdir, helperappfile)

                    pluginframeworkpath = self.dst_path_of('Chromium Embedded Framework.framework');
                    # Putting a Frameworks directory under Contents/MacOS
                    # isn't canonical, but the path baked into Dullahan
                    # Helper.app/Contents/MacOS/DullahanHelper is:
                    # @executable_path/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework
                    # (notice, not @executable_path/../Frameworks/etc.)
                    # So we'll create a symlink (below) from there back to the
                    # Frameworks directory nested under SLPlugin.app.
                    helperframeworkpath = \
                        self.dst_path_of('DullahanHelper.app/Contents/MacOS/'
                                         'Frameworks/Chromium Embedded Framework.framework')

                helperexecutablepath = self.dst_path_of('SLPlugin.app/Contents/Frameworks/DullahanHelper.app/Contents/MacOS/DullahanHelper')
                self.run_command_shell('install_name_tool -change '
                                 '"@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework" '
                                 '"@executable_path/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework" "%s"' % helperexecutablepath)

                # SLPlugin plugins
                with self.prefix(dst="llplugin"):
                    self.path2basename("../media_plugins/cef/" + self.args['configuration'],
                                       "media_plugin_cef.dylib")

                    # copy LibVLC plugin itself
                    self.path2basename("../media_plugins/libvlc/" + self.args['configuration'],
                                       "media_plugin_libvlc.dylib")

                    # copy LibVLC dynamic libraries
                    with self.prefix(src=os.path.join(self.args['build'], os.pardir, 'packages', 'lib', 'release' ), dst="lib"):
                        self.path("libvlc*.dylib*")

                    # copy LibVLC plugins folder
                    with self.prefix(src=os.path.join(self.args['build'], os.pardir, 'packages', 'lib', 'release', 'plugins' ), dst="lib"):
                        self.path("*.dylib")
                        self.path("plugins.dat")

                # do this install_name_tool *after* media plugin is copied over
                dylibexecutablepath = self.dst_path_of('llplugin/media_plugin_cef.dylib')
                self.run_command_shell('install_name_tool -change '
                                 '"@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework" '
                                 '"@executable_path/../Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework" "%s"' % dylibexecutablepath)

                #<FS:TS> Copy in prebuilt framework if it's there
                with self.prefix(src=os.path.join(self.args['build'], os.pardir, 'packages', 'bin_x86', 'Frameworks'), dst="Frameworks"):
                    self.path("Chromium Embedded Framework.framework")

            # CEF framework goes inside Second Life.app/Contents/Frameworks
            with self.prefix(dst="Frameworks"):
                frameworkfile="Chromium Embedded Framework.framework"
                self.path2basename(relpkgdir, frameworkfile)

            # This code constructs a relative path from the
            # target framework folder back to the location of the symlink.
            # It needs to be relative so that the symlink still works when
            # (as is normal) the user moves the app bundle out of the DMG
            # and into the /Applications folder. Note we also call 'raise'
            # to terminate the process if we get an error since without
            # this symlink, Second Life web media can't possibly work.
            # Real Framework folder:
            #   Second Life.app/Contents/Frameworks/Chromium Embedded Framework.framework/
            # Location of symlink and why it's relative 
            #   Second Life.app/Contents/Resources/SLPlugin.app/Contents/Frameworks/Chromium Embedded Framework.framework/
            # Real Frameworks folder, with the symlink inside the bundled SLPlugin.app (and why it's relative)
            #   <top level>.app/Contents/Frameworks/Chromium Embedded Framework.framework/
            #   <top level>.app/Contents/Resources/SLPlugin.app/Contents/Frameworks/Chromium Embedded Framework.framework ->
            # It might seem simpler just to create a symlink Frameworks to
            # the parent of Chromimum Embedded Framework.framework. But
            # that would create a symlink cycle, which breaks our
            # packaging step. So make a symlink from Chromium Embedded
            # Framework.framework to the directory of the same name, which
            # is NOT an ancestor of the symlink.
            frameworkpath = os.path.join(os.pardir, os.pardir, os.pardir,
                                         os.pardir, "Frameworks",
                                         "Chromium Embedded Framework.framework")
            try:
                # from SLPlugin.app/Contents/Frameworks/Chromium Embedded
                # Framework.framework back to Second
                # Life.app/Contents/Frameworks/Chromium Embedded Framework.framework
                origin, target = pluginframeworkpath, frameworkpath
                symlinkf(target, origin)
                # from SLPlugin.app/Contents/Frameworks/Dullahan
                # Helper.app/Contents/MacOS/Frameworks/Chromium Embedded
                # Framework.framework back to
                # SLPlugin.app/Contents/Frameworks/Chromium Embedded Framework.framework
                self.cmakedirs(os.path.dirname(helperframeworkpath))
                origin = helperframeworkpath
                target = os.path.join(os.pardir, frameworkpath)
                symlinkf(target, origin)
            except OSError as err:
                print "Can't symlink %s -> %s: %s" % (origin, target, err)
                raise


        # NOTE: the -S argument to strip causes it to keep enough info for
        # annotated backtraces (i.e. function names in the crash log).  'strip' with no
        # arguments yields a slightly smaller binary but makes crash logs mostly useless.
        # This may be desirable for the final release.  Or not.
        if ("package" in self.args['actions'] or 
            "unpacked" in self.args['actions']):
            self.run_command_shell('strip -S %(viewer_binary)r' %
                             { 'viewer_binary' : self.dst_path_of('Contents/MacOS/Firestorm')})
# </FS:Ansariel> construct method VMP trampoline crazy VMP launcher juggling shamelessly replaced with old version

    def package_finish(self):
        global CHANNEL_VENDOR_BASE
        # MBW -- If the mounted volume name changes, it breaks the .DS_Store's background image and icon positioning.
        #  If we really need differently named volumes, we'll need to create multiple DS_Store file images, or use some other trick.

        volname=CHANNEL_VENDOR_BASE+" Installer"  # DO NOT CHANGE without understanding comment above

        imagename = self.installer_base_name()

        sparsename = imagename + ".sparseimage"
        finalname = imagename + ".dmg"
        # make sure we don't have stale files laying about
        self.remove(sparsename, finalname)

        self.run_command(['hdiutil', 'create', sparsename,
                          '-volname', volname, '-fs', 'HFS+',
                          '-type', 'SPARSE', '-megabytes', '1300',
                          '-layout', 'SPUD'])

        # mount the image and get the name of the mount point and device node
        try:
            hdi_output = subprocess.check_output(['hdiutil', 'attach', '-private', sparsename])
        except subprocess.CalledProcessError as err:
            sys.exit("failed to mount image at '%s'" % sparsename)
            
        try:
            devfile = re.search("/dev/disk([0-9]+)[^s]", hdi_output).group(0).strip()
            volpath = re.search('HFS\s+(.+)', hdi_output).group(1).strip()

            if devfile != '/dev/disk1':
                # adding more debugging info based upon nat's hunches to the
                # logs to help track down 'SetFile -a V' failures -brad
                print "WARNING: 'SetFile -a V' command below is probably gonna fail"

            # Copy everything in to the mounted .dmg

            app_name = self.app_name()

            # Hack:
            # Because there is no easy way to coerce the Finder into positioning
            # the app bundle in the same place with different app names, we are
            # adding multiple .DS_Store files to svn. There is one for release,
            # one for release candidate and one for first look. Any other channels
            # will use the release .DS_Store, and will look broken.
            # - Ambroff 2008-08-20
            #<FS:TS> Select proper directory based on flavor and build type
            dmg_template_prefix = 'firestorm'
            if self.fs_is_opensim():
                dmg_template_prefix = 'firestormos'
            dmg_template = os.path.join(
                'installers', 'darwin', '%s-%s-dmg' % (dmg_template_prefix, self.channel_type()))
            print "Trying template directory", dmg_template

            if not os.path.exists (self.src_path_of(dmg_template)):
                dmg_template = os.path.join ('installers', 'darwin', 'release-dmg')
                print "Not found, trying template directory", dmg_template

            for s,d in {self.get_dst_prefix():app_name + ".app",
                        #os.path.join(dmg_template, "_VolumeIcon.icns"): ".VolumeIcon.icns",
                        os.path.join(dmg_template, "background.png"): "background.png",
                        os.path.join(dmg_template, "LGPL-license.txt"): "LGPL License.txt",
                        os.path.join(dmg_template, "VivoxAUP.txt"): "Vivox Acceptable Use Policy.txt",
                        os.path.join(dmg_template, "_DS_Store"): ".DS_Store"}.items():
                print "Copying to dmg", s, d
                self.copy_action(self.src_path_of(s), os.path.join(volpath, d))

            # <FS:TS> The next two commands *MUST* execute before the loop
            #         that hides the files. If not, packaging will fail.
            #         YOU HAVE BEEN WARNED.
            # Create the alias file (which is a resource file) from the .r
            self.run_command(
                ['Rez', self.src_path_of("%s/Applications-alias.r" % dmg_template),
                 '-o', os.path.join(volpath, "Applications")])

            # Set up the installer disk image: set icon positions, folder view
            #  options, and icon label colors. This must be done before the
            #  files are hidden.
            self.run_command(
                ['osascript',
                 self.src_path_of("installers/darwin/installer-dmg.applescript"),
                 volname])

            # <FS:TS> ARGH! osascript clobbers the volume icon file, for no
            #        reason I can find anywhere. So we need to copy it after
            #        running the script to set everything else up.
            print "Copying volume icon to dmg"
            self.copy_action(self.src_path_of(os.path.join(dmg_template, "_VolumeIcon.icns")),
                os.path.join(volpath, ".VolumeIcon.icns"))

            # Hide the background image, DS_Store file, and volume icon file (set their "visible" bit)
            for f in ".VolumeIcon.icns", "background.png", ".DS_Store":
                pathname = os.path.join(volpath, f)
                # We've observed mysterious "no such file" failures of the SetFile
                # command, especially on the first file listed above -- yet
                # subsequent inspection of the target directory confirms it's
                # there. Timing problem with copy command? Try to handle.
                for x in xrange(3):
                    if os.path.exists(pathname):
                        print "Confirmed existence: %r" % pathname
                        break
                    print "Waiting for %s copy command to complete (%s)..." % (f, x+1)
                    sys.stdout.flush()
                    time.sleep(1)
                # If we fall out of the loop above without a successful break, oh
                # well, possibly we've mistaken the nature of the problem. In any
                # case, don't hang up the whole build looping indefinitely, let
                # the original problem manifest by executing the desired command.
                self.run_command(['SetFile', '-a', 'V', pathname])

            # Set the alias file's alias and custom icon bits
            self.run_command(['SetFile', '-a', 'AC', os.path.join(volpath, "Applications")])

            # Set the disk image root's custom icon bit
            self.run_command(['SetFile', '-a', 'C', volpath])

            # Sign the app if requested; 
            # do this in the copy that's in the .dmg so that the extended attributes used by 
            # the signature are preserved; moving the files using python will leave them behind
            # and invalidate the signatures.
            if 'signature' in self.args:
                app_in_dmg=os.path.join(volpath,self.app_name()+".app")
                print "Attempting to sign '%s'" % app_in_dmg
                identity = self.args['signature']
                if identity == '':
                    identity = 'Developer ID Application'

                # Look for an environment variable set via build.sh when running in Team City.
                try:
                    build_secrets_checkout = os.environ['build_secrets_checkout']
                except KeyError:
                    pass
                else:
                    # variable found so use it to unlock keychain followed by codesign
                    home_path = os.environ['HOME']
                    keychain_pwd_path = os.path.join(build_secrets_checkout,'code-signing-osx','password.txt')
                    keychain_pwd = open(keychain_pwd_path).read().rstrip()

                    # Note: As of macOS Sierra, keychains are created with names postfixed with '-db' so for example, the
                    #       SL Viewer keychain would by default be found in ~/Library/Keychains/viewer.keychain-db instead of
                    #       just ~/Library/Keychains/viewer.keychain in earlier versions.
                    #
                    #       Because we have old OS files from previous versions of macOS on the build hosts, the configurations
                    #       are different on each host. Some have viewer.keychain, some have viewer.keychain-db and some have both.
                    #       As you can see in the line below, this script expects the Linden Developer cert/keys to be in viewer.keychain.
                    #
                    #       To correctly sign builds you need to make sure ~/Library/Keychains/viewer.keychain exists on the host
                    #       and that it contains the correct cert/key. If a build host is set up with a clean version of macOS Sierra (or later)
                    #       then you will need to change this line (and the one for 'codesign' command below) to point to right place or else
                    #       pull in the cert/key into the default viewer keychain 'viewer.keychain-db' and export it to 'viewer.keychain'
                    viewer_keychain = os.path.join(home_path, 'Library',
                                                   'Keychains', 'viewer.keychain')
                    self.run_command(['security', 'unlock-keychain',
                                      '-p', keychain_pwd, viewer_keychain])
                    signed=False
                    sign_attempts=3
                    sign_retry_wait=15
                    while (not signed) and (sign_attempts > 0):
                        try:
                            sign_attempts-=1;
                            self.run_command(
                                # Note: See blurb above about names of keychains
                               ['codesign', '--verbose', '--deep', '--force',
                                '--keychain', viewer_keychain, '--sign', identity,
                                app_in_dmg])
                            signed=True # if no exception was raised, the codesign worked
                        except ManifestError as err:
                            if sign_attempts:
                                print >> sys.stderr, "codesign failed, waiting %d seconds before retrying" % sign_retry_wait
                                time.sleep(sign_retry_wait)
                                sign_retry_wait*=2
                            else:
                                print >> sys.stderr, "Maximum codesign attempts exceeded; giving up"
                                raise
                    self.run_command(['spctl', '-a', '-texec', '-vv', app_in_dmg])

            imagename="SecondLife_" + '_'.join(self.args['version'])


        finally:
            # Unmount the image even if exceptions from any of the above 
            self.run_command(['hdiutil', 'detach', '-force', devfile])

        print "Converting temp disk image to final disk image"
        self.run_command(['hdiutil', 'convert', sparsename, '-format', 'UDZO',
                          '-imagekey', 'zlib-level=9', '-o', finalname])
        self.run_command(['hdiutil', 'internet-enable', '-yes', finalname])
        # get rid of the temp file
        self.package_file = finalname
        self.remove(sparsename)
        self.fs_save_osx_symbols()

class Darwin_i386_Manifest(DarwinManifest):
    address_size = 32


class Darwin_i686_Manifest(DarwinManifest):
    """alias in case arch is passed as i686 instead of i386"""
    pass


class Darwin_x86_64_Manifest(DarwinManifest):
    address_size = 64


class LinuxManifest(ViewerManifest):
    build_data_json_platform = 'lnx'

    def construct(self):
        super(LinuxManifest, self).construct()

        pkgdir = os.path.join(self.args['build'], os.pardir, 'packages')
        relpkgdir = os.path.join(pkgdir, "lib", "release")
        debpkgdir = os.path.join(pkgdir, "lib", "debug")

        self.path("licenses-linux.txt","licenses.txt")
        self.path("VivoxAUP.txt")
        self.path("res/firestorm_icon.png","firestorm_icon.png")
        with self.prefix("linux_tools"):
            self.path("client-readme.txt","README-linux.txt")
            self.path("FIRESTORM_DESKTOPINSTALL.txt","FIRESTORM_DESKTOPINSTALL.txt")
            self.path("client-readme-voice.txt","README-linux-voice.txt")
            self.path("client-readme-joystick.txt","README-linux-joystick.txt")
            self.path("wrapper.sh","firestorm")
            with self.prefix(dst="etc"):
                self.path("handle_secondlifeprotocol.sh")
                self.path("register_secondlifeprotocol.sh")
                self.path("refresh_desktop_app_entry.sh")
                self.path("launch_url.sh")
            self.path("install.sh")

        with self.prefix(dst="bin"):
            self.path("firestorm-bin","do-not-directly-run-firestorm-bin")
            self.path("../linux_crash_logger/linux-crash-logger","linux-crash-logger.bin")
            self.path2basename("../llplugin/slplugin", "SLPlugin") 
            #this copies over the python wrapper script, associated utilities and required libraries, see SL-321, SL-322 and SL-323
            # <FS:Ansariel> Remove VMP
            #with self.prefix(src="../viewer_components/manager"):
            #    self.path("SL_Launcher")
            #    self.path("*.py")
            #with self.prefix(src=os.path.join("lib", "python", "llbase"), dst="llbase"):
            #    self.path("*.py")
            #    self.path("_cllsd.so")         
            # </FS:Ansariel> Remove VMP

        # recurses, packaged again
        self.path("res-sdl")

        # Get the icons based on the channel type
        icon_path = self.icon_path()
        print "DEBUG: icon_path '%s'" % icon_path
        with self.prefix(src=icon_path) :
            self.path("firestorm_256.png","firestorm_48.png")
            with self.prefix(dst="res-sdl") :
                self.path("firestorm_256.BMP","ll_icon.BMP")

        # plugins
        with self.prefix(src=os.path.join(self.args['build'], os.pardir, 'media_plugins', dst="bin/llplugin")):
            self.path("gstreamer010/libmedia_plugin_gstreamer010.so",
                      "libmedia_plugin_gstreamer.so")
            self.path2basename("libvlc", "libmedia_plugin_libvlc.so")
            self.path("cef/libmedia_plugin_cef.so", "libmedia_plugin_cef.so" )


        with self.prefix(src=os.path.join(pkgdir, 'lib', 'vlc', 'plugins'), dst="bin/llplugin/vlc/plugins"):
            self.path( "plugins.dat" )
            self.path( "*/*.so" )

        with self.prefix(src=os.path.join(pkgdir, 'lib' ), dst="lib"):
            self.path( "libvlc*.so*" )

        with self.prefix(src=os.path.join(pkgdir, 'lib', 'vlc', 'plugins'), dst="bin/llplugin/vlc/plugins"):
            self.path( "plugins.dat" )
            self.path( "*/*.so" )

        with self.prefix(src=os.path.join(pkgdir, 'lib' ), dst="lib"):
            self.path( "libvlc*.so*" )

        # CEF files 
        with self.prefix(src=os.path.join(pkgdir, 'lib', 'release'), dst="lib"):
            self.path( "libcef.so" )
            self.path( "libllceflib.so" )
            
        with self.prefix(src=os.path.join(pkgdir, 'release', 'swiftshader'), dst=os.path.join("bin", "swiftshader") ):
            self.path( "*.so" )

        with self.prefix(src=os.path.join(pkgdir, 'bin', 'release'), dst="bin"):
            self.path( "chrome-sandbox" )
            self.path( "dullahan_host" )
            self.path( "natives_blob.bin" )
            self.path( "snapshot_blob.bin" )
            self.path( "v8_context_snapshot.bin" )
            self.path( "libffmpegsumo.so" )

        with self.prefix(src=os.path.join(pkgdir, 'resources'), dst="bin"):
            self.path( "cef.pak" )
            self.path( "cef_extensions.pak" )
            self.path( "cef_100_percent.pak" )
            self.path( "cef_200_percent.pak" )
            self.path( "devtools_resources.pak" )
            self.path( "icudtl.dat" )

        with self.prefix(src=os.path.join(pkgdir, 'resources', 'locales'), dst=os.path.join('bin', 'locales')):
            self.path("am.pak")
            self.path("ar.pak")
            self.path("bg.pak")
            self.path("bn.pak")
            self.path("ca.pak")
            self.path("cs.pak")
            self.path("da.pak")
            self.path("de.pak")
            self.path("el.pak")
            self.path("en-GB.pak")
            self.path("en-US.pak")
            self.path("es-419.pak")
            self.path("es.pak")
            self.path("et.pak")
            self.path("fa.pak")
            self.path("fi.pak")
            self.path("fil.pak")
            self.path("fr.pak")
            self.path("gu.pak")
            self.path("he.pak")
            self.path("hi.pak")
            self.path("hr.pak")
            self.path("hu.pak")
            self.path("id.pak")
            self.path("it.pak")
            self.path("ja.pak")
            self.path("kn.pak")
            self.path("ko.pak")
            self.path("lt.pak")
            self.path("lv.pak")
            self.path("ml.pak")
            self.path("mr.pak")
            self.path("ms.pak")
            self.path("nb.pak")
            self.path("nl.pak")
            self.path("pl.pak")
            self.path("pt-BR.pak")
            self.path("pt-PT.pak")
            self.path("ro.pak")
            self.path("ru.pak")
            self.path("sk.pak")
            self.path("sl.pak")
            self.path("sr.pak")
            self.path("sv.pak")
            self.path("sw.pak")
            self.path("ta.pak")
            self.path("te.pak")
            self.path("th.pak")
            self.path("tr.pak")
            self.path("uk.pak")
            self.path("vi.pak")
            self.path("zh-CN.pak")
            self.path("zh-TW.pak")

        # llcommon
        if not self.path("../llcommon/libllcommon.so", "lib/libllcommon.so"):
            print "Skipping llcommon.so (assuming llcommon was linked statically)"

        self.path("featuretable_linux.txt")

        with self.prefix(src=pkgdir, dst="bin"):
            self.path("ca-bundle.crt")

        if self.is_packaging_viewer():
          with self.prefix(src=os.path.join(pkgdir, 'lib', 'release'), dst="lib"):
            self.path("libapr-1.so*")
            self.path("libaprutil-1.so*")
            self.path("libboost_context-mt.so*")
            self.path("libboost_filesystem-mt.so*")
            self.path("libboost_program_options-mt.so*")
            self.path("libboost_regex-mt.so*")
            self.path("libboost_signals-mt.so*")
            self.path("libboost_system-mt.so*")
            self.path("libboost_thread-mt.so*")
            self.path("libboost_chrono-mt.so*") #<FS:TM> FS spcific
            self.path("libboost_date_time-mt.so*") #<FS:TM> FS spcific
            self.path("libboost_wave-mt.so*") #<FS:TM> FS spcific
            self.path("libcollada14dom.so*")
            self.path("libdb*.so*")
            self.path("libcrypto.so*")
            self.path("libexpat.so*")
            self.path("libssl.so*")
            self.path("libGLOD.so")
            self.path("libminizip.so")
            self.path("libuuid.so*")
            self.path("libSDL-1.2.so*")
            self.path("libdirectfb*.so*")
            self.path("libfusion*.so*")
            self.path("libdirect*.so*")
            self.path("libopenjpeg.so*")
            self.path("libhunspell-1.3.so*")
            self.path("libalut.so*")
            self.path("libpng15.so.15") #use provided libpng to workaround incompatible system versions on some distros
            self.path("libpng15.so.15.13.0") #use provided libpng to workaround incompatible system versions on some distros
            self.path("libpng15.so.15.1.0") #use provided libpng to workaround incompatible system versions on some distros
            self.path("libopenal.so", "libopenal.so.1") # Install as versioned file in case it's missing from the 3p- and won't get copied below
            self.path("libopenal.so*")
            #self.path("libnotify.so.1.1.2", "libnotify.so.1") # LO - uncomment when testing libnotify(growl) on linux
            self.path("libpangox-1.0.so*")
            # KLUDGE: As of 2012-04-11, the 'fontconfig' package installs
            # libfontconfig.so.1.4.4, along with symlinks libfontconfig.so.1
            # and libfontconfig.so. Before we added support for library-file
            # wildcards, though, this self.path() call specifically named
            # libfontconfig.so.1.4.4 WITHOUT also copying the symlinks. When I
            # (nat) changed the call to self.path("libfontconfig.so.*"), we
            # ended up with the libfontconfig.so.1 symlink in the target
            # directory as well. But guess what! At least on Ubuntu 10.04,
            # certain viewer fonts look terrible with libfontconfig.so.1
            # present in the target directory. Removing that symlink suffices
            # to improve them. I suspect that means we actually do better when
            # the viewer fails to find our packaged libfontconfig.so*, falling
            # back on the system one instead -- but diagnosing and fixing that
            # is a bit out of scope for the present project. Meanwhile, this
            # particular wildcard specification gets us exactly what the
            # previous call did, without having to explicitly state the
            # version number.
            self.path("libfontconfig.so.*.*")
            try:
                self.path("libtcmalloc.so*") #formerly called google perf tools
                pass
            except:
                print "tcmalloc files not found, skipping"
                pass

          # Vivox runtimes
          # Currentelly, the 32-bit ones will work with a 64-bit client.
          with self.prefix(src=os.path.join(pkgdir, 'lib', 'release'), dst="bin"):
                  self.path("SLVoice")
                  self.path("win32")

          with self.prefix(src=os.path.join(pkgdir, 'lib', 'release'), dst="lib"):
                  self.path("libortp.so")
                  self.path("libsndfile.so.1")
                  # <FS:TS> Vivox wants this library even if it's present already in the viewer
                  self.path("libvivoxoal.so.1")
                  self.path("libvivoxsdk.so")
                  self.path("libvivoxplatform.so")


    def package_finish(self):
        # a standard map of strings for replacing in the templates
        installer_name_components = ['Phoenix',self.app_name(),self.args.get('arch'),'.'.join(self.args['version'])]
        installer_name = "_".join(installer_name_components)
        #installer_name = self.installer_base_name()

        self.fs_delete_linux_symbols() # <FS:ND/> Delete old syms
        self.strip_binaries()
        self.fs_save_linux_symbols() # <FS:ND/> Package symbols, add debug link
        self.fs_setuid_chromesandbox() # <FS:ND/> Chown chrome-sandbox to root:root and set the setuid bit

        # Fix access permissions
        self.run_command(['find', self.get_dst_prefix(),
                          '-type', 'd', '-exec', 'chmod', '755', '{}', ';'])
        for old, new in ('0700', '0755'), ('0500', '0555'), ('0600', '0644'), ('0400', '0444'):
            self.run_command(['find', self.get_dst_prefix(),
                              '-type', 'f', '-perm', old,
                              '-exec', 'chmod', new, '{}', ';'])
        self.package_file = installer_name + '.tar.xz'

        # temporarily move directory tree so that it has the right
        # name in the tarfile
        realname = self.get_dst_prefix()
        tempname = self.build_path_of(installer_name)
        self.run_command(["mv", realname, tempname])
        try:
            # only create tarball if it's a release build.
            if self.args['buildtype'].lower() == 'release':
                # --numeric-owner hides the username of the builder for
                # security etc.
                self.run_command(['tar', '-C', self.get_build_prefix(),
                                  '--numeric-owner', self.fs_linux_tar_excludes(), '-caf',
                                 tempname + '.tar.xz', installer_name])
            else:
                print "Skipping %s.tar.xz for non-Release build (%s)" % \
                      (installer_name, self.args['buildtype'])
        finally:
            self.run_command(["mv", tempname, realname])

    def strip_binaries(self):
        if self.args['buildtype'].lower() == 'release' and self.is_packaging_viewer():
            print "* Going strip-crazy on the packaged binaries, since this is a RELEASE build"
            # makes some small assumptions about our packaged dir structure
            self.run_command(
                ["find"] +
                [os.path.join(self.get_dst_prefix(), dir) for dir in ('bin', 'lib')] +
                # <FS:Ansariel> Remove VMP
                #['-type', 'f', '!', '-name', '*.py', '!', '-name', 'SL_Launcher',
                ['-type', 'f', "!", "-name", "*.dat", "!", "-name", "*.pak", "!", "-name", "*.bin",
                # </FS:Ansariel> Remove VMP
                 '!', '-name', 'update_install', '-exec', 'strip', '-S', '{}', ';'])

class Linux_i686_Manifest(LinuxManifest):
    address_size = 32

    def construct(self):
        super(Linux_i686_Manifest, self).construct()

        pkgdir = os.path.join(self.args['build'], os.pardir, 'packages')
        relpkgdir = os.path.join(pkgdir, "lib", "release")
        debpkgdir = os.path.join(pkgdir, "lib", "debug")

        with self.prefix(src=relpkgdir, dst="lib"):
            self.path("libapr-1.so")
            self.path("libapr-1.so.0")
            self.path("libapr-1.so.0.4.5")
            self.path("libaprutil-1.so")
            self.path("libaprutil-1.so.0")
            self.path("libaprutil-1.so.0.4.1")
            self.path("libdb*.so")
            self.path("libexpat.so.*")
            self.path("libGLOD.so")
            self.path("libuuid.so*")
            self.path("libSDL-1.2.so.*")
            self.path("libdirectfb-1.*.so.*")
            self.path("libfusion-1.*.so.*")
            self.path("libdirect-1.*.so.*")
            self.path("libopenjpeg.so*")
            self.path("libdirectfb-1.4.so.5")
            self.path("libfusion-1.4.so.5")
            self.path("libdirect-1.4.so.5*")
            self.path("libhunspell-1.3.so*")
            self.path("libalut.so*")
            self.path("libopenal.so*")

            # <FS:ND> Linking this breaks voice as stock openal.so does not have alcGetMixedBuffer
            #self.path("libopenal.so", "libvivoxoal.so.1") # vivox's sdk expects this soname
            # </FS:ND>
            
            # KLUDGE: As of 2012-04-11, the 'fontconfig' package installs
            # libfontconfig.so.1.4.4, along with symlinks libfontconfig.so.1
            # and libfontconfig.so. Before we added support for library-file
            # wildcards, though, this self.path() call specifically named
            # libfontconfig.so.1.4.4 WITHOUT also copying the symlinks. When I
            # (nat) changed the call to self.path("libfontconfig.so.*"), we
            # ended up with the libfontconfig.so.1 symlink in the target
            # directory as well. But guess what! At least on Ubuntu 10.04,
            # certain viewer fonts look terrible with libfontconfig.so.1
            # present in the target directory. Removing that symlink suffices
            # to improve them. I suspect that means we actually do better when
            # the viewer fails to find our packaged libfontconfig.so*, falling
            # back on the system one instead -- but diagnosing and fixing that
            # is a bit out of scope for the present project. Meanwhile, this
            # particular wildcard specification gets us exactly what the
            # previous call did, without having to explicitly state the
            # version number.
            self.path("libfontconfig.so.*.*")

            # Include libfreetype.so. but have it work as libfontconfig does.
            self.path("libfreetype.so.*.*")

            try:
                self.path("libtcmalloc.so*") #formerly called google perf tools
                pass
            except:
                print "tcmalloc files not found, skipping"
                pass

            try:
                self.path("libfmodex-*.so")
                self.path("libfmodex.so")
                self.path("libfmodex.so*")
                pass
            except:
                print "Skipping libfmodex.so - not found"
                pass

            try:
                self.path("libfmod-*.so")
                self.path("libfmod.so")
                self.path("libfmod.so*")
                pass
            except:
                print "Skipping libfmodstudio.so - not found"
                pass


        # Vivox runtimes
        with self.prefix(src=relpkgdir, dst="bin"):
            self.path("SLVoice")
        with self.prefix(src=relpkgdir, dst="lib"):
            self.path("libortp.so")
            self.path("libsndfile.so.1")
            #self.path("libvivoxoal.so.1") # no - we'll re-use the viewer's own OpenAL lib
            self.path("libvivoxsdk.so")
            self.path("libvivoxplatform.so")

        self.fs_delete_linux_symbols() # <FS:ND/> Delete old syms
        self.strip_binaries()


class Linux_x86_64_Manifest(LinuxManifest):
    address_size = 64

    def construct(self):
        super(Linux_x86_64_Manifest, self).construct()

        if self.is_packaging_viewer():
          with self.prefix(src=os.path.join(pkgdir, 'lib', 'release'), dst="lib"):
            self.path("libffi*.so*")
            # vivox 32-bit hack.
            # one has to extract libopenal.so from the 32-bit openal package, or official LL viewer, and rename it to libopenal32.so
            # and place it in the prebuilt lib/release directory
            # <FS:TS> No, we don't need to dink with this. A usable library
            # is now in the slvoice package, and we need to just use it as is.
            # self.path("libopenal32.so", "libvivoxoal.so.1") # vivox's sdk expects this soname
            try:
                    self.path("libfmodex64-*.so")
                    self.path("libfmodex64.so")
                    self.path("libfmodex64.so*")
                    pass
            except:
                    print "Skipping libfmodex.so - not found"
                    pass

            try:
                self.path("libfmod-*.so")
                self.path("libfmod.so")
                self.path("libfmod.so*")
                pass
            except:
                print "Skipping libfmod.so - not found"
                pass

          self.prefix(src=os.path.join(pkgdir, 'lib', 'release', 'x64'), dst="lib")
          try:
              self.path("libLeap.so")
          except:
              print "Leap Motion library not found"
          self.end_prefix("lib")

        with self.prefix(dst="bin"):
            self.path2basename("../llplugin/slplugin", "SLPlugin")

        # plugins
        with self.prefix(dst="bin/llplugin"):
            self.path2basename("../media_plugins/webkit", "libmedia_plugin_webkit.so")
            self.path("../media_plugins/gstreamer010/libmedia_plugin_gstreamer010.so", "libmedia_plugin_gstreamer.so")

        self.path("secondlife-i686.supp")

################################################################
# <FS:Ansariel> Added back for Mac compatibility reason
def symlinkf(src, dst):
    """
    Like ln -sf, but uses os.symlink() instead of running ln.
    """
    try:
        os.symlink(src, dst)
    except OSError as err:
        if err.errno != errno.EEXIST:
            raise
        # We could just blithely attempt to remove and recreate the target
        # file, but that strategy doesn't work so well if we don't have
        # permissions to remove it. Check to see if it's already the
        # symlink we want, which is the usual reason for EEXIST.
        elif os.path.islink(dst):
            if os.readlink(dst) == src:
                # the requested link already exists
                pass
            else:
                # dst is the wrong symlink; attempt to remove and recreate it
                os.remove(dst)
                os.symlink(src, dst)
        elif os.path.isdir(dst):
            print "Requested symlink (%s) exists but is a directory; replacing" % dst
            shutil.rmtree(dst)
            os.symlink(src, dst)
        elif os.path.exists(dst):
            print "Requested symlink (%s) exists but is a file; replacing" % dst
            os.remove(dst)
            os.symlink(src, dst)
        else:
            # see if the problem is that the parent directory does not exist
            # and try to explain what is missing
            (parent, tail) = os.path.split(dst)
            while not os.path.exists(parent):
                (parent, tail) = os.path.split(parent)
            if tail:
                raise Exception("Requested symlink (%s) cannot be created because %s does not exist"
                                % os.path.join(parent, tail))
            else:
                raise
# </FS:Ansariel> Added back for Mac compatibility reason

if __name__ == "__main__":
    extra_arguments = [
        dict(name='bugsplat', description="""BugSplat database to which to post crashes,
             if BugSplat crash reporting is desired""", default=''),
        ]
    main(extra=extra_arguments)
