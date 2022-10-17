class MacFuseRequirement < Requirement
  fatal true

  satisfy(build_env: false) { self.class.binary_mac_fuse_installed? }

  def self.binary_mac_fuse_installed?
    File.exist?("/usr/local/include/fuse/fuse.h") &&
      !File.symlink?("/usr/local/include/fuse")
  end

  env do
    ENV.append_path "PKG_CONFIG_PATH", HOMEBREW_LIBRARY/"Homebrew/os/mac/pkgconfig/fuse"
    ENV.append_path "PKG_CONFIG_PATH", "/usr/local/lib/pkgconfig"

    unless HOMEBREW_PREFIX.to_s == "/usr/local"
      ENV.append_path "HOMEBREW_LIBRARY_PATHS", "/usr/local/lib"
      ENV.append_path "HOMEBREW_INCLUDE_PATHS", "/usr/local/include/fuse"
    end
  end

  def message
    "macFUSE is required. Please run `brew install --cask macfuse` first."
  end
end

class Ext4fuse < Formula
  desc "Read-only implementation of ext4 for FUSE"
  homepage "https://github.com/gerard/ext4fuse"
  url "https://github.com/gerard/ext4fuse/archive/v0.1.3.tar.gz"
  sha256 "550f1e152c4de7d4ea517ee1c708f57bfebb0856281c508511419db45aa3ca9f"
  license "GPL-2.0"
  head "https://github.com/gerard/ext4fuse.git"

  bottle do
    sha256 cellar: :any, catalina:    "446dde5e84b058966ead0cde5e38e9411f465732527f6decfa1c0dcdbd4abbef"
    sha256 cellar: :any, mojave:      "88c4918bf5218f99295e539fe4499152edb3b60b6659e44ddd68b22359f512ae"
    sha256 cellar: :any, high_sierra: "fc69c8993afd0ffc16a73c9c036ca8f83c77ac2a19b3237f76f9ccee8b30bbc9"
    sha256 cellar: :any, sierra:      "fe8bbe7cd5362f00ff06ef750926bf349d60563c20b0ecf212778631c8912ba2"
    sha256 cellar: :any, el_capitan:  "291047c821b7b205d85be853fb005510c6ab01bd4c2a2193c192299b6f049d35"
    sha256 cellar: :any, yosemite:    "b11f564b7e7c08af0b0a3e9854973d39809bf2d8a56014f4882772b2f7307ac1"
  end

  depends_on "pkg-config" => :build

  on_macos do
    depends_on MacFuseRequirement => :build
  end

  on_linux do
    depends_on "libfuse"
  end

  def install
    system "make"
    bin.install "ext4fuse"
  end
end
