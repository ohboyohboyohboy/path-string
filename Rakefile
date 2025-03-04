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

Hoe.plugin :bundler
Hoe.plugin :gemspec
Hoe.plugin :version
Hoe.plugins.delete(:test)

Hoe.spec PROJECT_NAME do
  developer("Kyle Yetter", "kyle@ohboyohboyohboy.org")
  license "MIT"

  self.version     = Path::VERSION
  self.readme_file = "README.rdoc"

  self.extra_dev_deps       <<
    %w( rake-compiler >=0 ) <<
    %w( rspec ~>2.14 )      <<
    %w( rake ~>10.4 )

  self.spec_extras = {
    extensions: %w( ext/path_string/extconf.rb ),
    required_ruby_version: '>= 1.9'
  }

  self.clean_globs += %w(
    ext/path_string/*.{o,so}
    ext/path_string/Makefile
    lib/path_string/*.so
  )

  Rake::ExtensionTask.new('path_string_native', spec) do |ext|
    ext.ext_dir = 'ext/path_string'
    ext.lib_dir = File.join('lib', 'path_string')
  end
end

