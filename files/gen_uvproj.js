const fs = require('fs')
const path = require('path')

function gen() {
  let result = ''
  let files = fs.readFileSync('files_m47.txt').toString().split('\n')

  files.forEach(file => {
    file = file.trim();
    const name = path.basename(file);
    if(!file) {
      return;
    }

    let type = 1;
    if(file.endsWith('.cpp') || file.endsWith('.cxx') || file.endsWith('.cc')) {
      type = 8;
    }

    result += 
`            <File>
                <FileName>${name}</FileName>
                <FileType>${type}</FileType>
                <FilePath>${file}</FilePath>
             </File>
`              
  })
  fs.writeFileSync("files_m47.xml", result);
  console.log("save to files_m47.xml");
}  

gen()
