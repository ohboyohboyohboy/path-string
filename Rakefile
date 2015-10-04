#!/usr/bin/ruby
# encoding: utf-8
#
# author: Kyle Yetter
#

this_dir = File.expand_path('..', __FILE__)
lib_dir  = File.join(this_dir, 'lib')
$LOAD_PATH.unshift(lib_dir) unless $LOAD_PATH.include?(lib_dir)

require 'path_string/version'
require 'rubygems'
require 'hoe'
require 'rake/extensiontask'

PROJECT_NAME = 'path-string'

Hoe.spec PROJECT_NAME do
  developer("Kyle Yetter", "kyle@ohboyohboyohboy.org")
  license "MIT"

  self.version     = Path::VERSION
  self.readme_file = "README.rdoc"

  self.extra_dev_deps       <<
    %w( rake-compiler >=0 ) <<
    %w( rspec ~>2.14 )      <<
    %w( rake ~>10.4 )

  Rake::ExtensionTask.new('path_string_native', spec) do |ext|
    ext.ext_dir = 'ext/path_string'
    ext.lib_dir = File.join('lib', 'path_string')
  end
end

#tasks = Rake.application.instance_variable_get( :@tasks )
#%w( release release_to_gemcutter release_to_rubyforge announce publish_docs ).each do | task_name |
#  tasks.delete( task_name )
#end
#
#desc "Upload gem to gems.ohboyohboyohboy.org and update the gem index"
#task :release => [ :package ] do
#  sh( "scp pkg/*.gem gems@ohboyohboyohboy.org:public_html/gems/" )
#  sh( "ssh gems@ohboyohboyohboy.org 'cd public_html; gem generate_index;'" )
#end
