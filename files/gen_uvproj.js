const fs = require('fs')
const path = require('path')

function gen() {
  let result = ''
  let files = fs.readFileSync('files_m47.txt').toString().split('\n')

  files.forEach(file => {
    const name = path.basename(file);
    if(!file) {
      return;
    }
    result += 
`            <File>
                <FileName>${name}</FileName>
                <FileType>1</FileType>
                <FilePath>${file}</FilePath>
             </File>
`              
  })
  fs.writeFileSync("files_m47.xml", result);
  console.log("save to files_m47.xml");
}  

gen()
