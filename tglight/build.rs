extern crate cc;
// extern crate pkg_config;
use std::env;
use std::path::{Path, PathBuf};
use std::fs::canonicalize;
use std::io;
use std::fs::read_dir;

fn collect_files(dir: &Path, out: &mut Vec<PathBuf>) -> Result<(), io::Error>{
    if dir.is_dir() {
        for entry in read_dir(dir)? {
            let entry = entry?;
            let path = entry.path();
            if path.is_dir() {
                collect_files(&path, out)?;
            } else {
                out.push(path);
            }
        }
    }
    Ok(())
}

fn main() {
      let project_dir = canonicalize(PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap())).unwrap();
      let jazzlights_dir = canonicalize(match env::var("JAZZLIGHTS_DIR") {
            Ok(v) => PathBuf::from(v),
            Err(_e) => project_dir.join("..")
      }).unwrap();
      // let out_dir = canonicalize(PathBuf::from(env::var("OUT_DIR").unwrap())).unwrap();
      // let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
      //env::set_var("PKG_CONFIG_ALLOW_CROSS", "1");
      // pkg_config::probe_library("glfw3").unwrap();
      // pkg_config::Config::new().statik(true).probe("glfw3").unwrap();

      let mut cpp_files = Vec::<PathBuf>::new();
      collect_files(&project_dir.join("src"), &mut cpp_files).unwrap();
      collect_files(&jazzlights_dir.join("src"), &mut cpp_files).unwrap();
      collect_files(&jazzlights_dir.join("extras/jazzlights"), &mut cpp_files).unwrap();
      let cpp_files : Vec<&PathBuf> = cpp_files.iter()
        .filter(|&f| f.to_str().unwrap().ends_with(".cpp"))
        .collect();
      eprintln!("Compiling {:?}", cpp_files);

      cc::Build::new()
            .cpp(true)
            .warnings(true) // "-Wall -Wextra for GCC/Clang, /Wall for MSVC"
            .flag("-std=c++14")
            .flag("-DBOOT_NAME=TGLIGHT")
            .include(project_dir.join("src"))
            .include(jazzlights_dir.join("src"))
            .include(jazzlights_dir.join("extras"))
            .files(cpp_files)
            .compile("tgplayer");
}
